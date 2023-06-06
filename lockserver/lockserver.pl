#!/usr/bin/perl

# testing:
#   socat - pty,link=testpty
#   ./lockserver.pl --device testpty --stdio --logfile - --passwdfile testpwd.shadow --unix-socket test.sock
#     # pin is 12341234
#   echo -n '!0' | socat -u - UNIX-SENDTO:test.sock
#   socat - UNIX-SENDTO:test.sock,bind=""

# DONE: Getopt::Long

use strict;
use warnings;

use IO::Handle;
use IO::Select;
use POSIX qw(EAGAIN EWOULDBLOCK strftime);
use IO::Socket::UNIX;
use Getopt::Long qw(:config bundling);
#use Device::SerialPort;
#use DateTime;
#don't use Authen::Simple::Passwd, it is dangerously flawed;

## "myextlib"?
##use Inline C => <<'EOC', CCFLAGS => "-std=c99 -O3",LIBS => "./verify.o";
##use Inline C => <<'EOC', CCFLAGS => "-std=c99 -O3 $ENV{PWD}/verify.o";
#use Inline C => <<'EOC', CCFLAGS => "-std=c99 -O3",myextlib => "$ENV{PWD}/libverify.so";
#
#int verify_pinpad(char* user, size_t user_len, char* pw, size_t pw_len);
#int c_verify_pinpad(char* user, size_t user_len, char* pw, size_t pw_len) {
#  return verify_pinpad(user,user_len,pw,pw_len);
#}
#
#EOC

#c_verify_pinpad("test",4,"test",4);

#my $ttyfile = shift || "/dev/ttyUSB0";
#my $initial_baudrate = 0+(shift || 9600);
#my $ux_path = shift || "/run/lockserver.sock";
#my $logfile = shift || "/var/log/lockserver.log";
#my $passwd_file = shift || "/var/schluessel/pws.shadow";

my $ttyfile = "/dev/ttyUSB0";
my $initial_baudrate = 9600;
my $ux_path = "/run/lockserver.sock";
my $logfile = "/var/log/lockserver.log";
my $passwd_file = "/var/schluessel/pws.shadow";

my $use_stdio = 0;
my $log_everything = 0; #1;
#my $server_group = undef;
my $server_group = "www-data";

my (@opts,%opts,%opts_explained);

sub usage {
  my $ret = shift//0;
  if ($ret != 0) {
    print STDERR "wrong parameter. Left are: ",join(" ",@ARGV),"\n";
  }
  #print join("\n  --",$0,@opts),"\n";
  print STDERR "usage:\n  $0\n";
  for (@opts) {
    my $name = $_ =~ s/[|!=:].*//r;
    my $value = $opts{$name}//"undefined";
    if (ref $value eq "SCALAR") {
      $value = $$value;
    } elsif (ref $value eq "CODE") {
      $value = undef;
    }
    my $explanation = $opts_explained{$name};
    print STDERR "    --",$_,(defined $value ? " (value: $value)":""),"\n",
          defined($explanation) ? "        $explanation\n":"";
  }
  exit($ret);
}

%opts = (
  stdio => \$use_stdio,
  "log-everything" => \$log_everything,
  debug => 0,
  device => \$ttyfile,
  baudrate => \$initial_baudrate,
  "unix-socket" => \$ux_path,
  "unix-group" => \$server_group,
  logfile => \$logfile,
  passwdfile => \$passwd_file,
  pidfile => "",
  "retry-on-error" => 0,
  help => sub { usage(0) },
);

%opts_explained = (
  stdio => "Listen to stdin and write to stdout.",
  "log-everything" => "Log every line coming from the device.",
  debug => "Enable debug logging output.",
  device => "Use this serial device to connect to the fablock hardware.",
  baudrate => "Set the initial baudrate upon (re-)connecting.",
  "unix-socket" => "The unix domain socket to listen for datagrams.",
  "unix-group" => "Make the socket writable for this group.",
  logfile => "Path of the logfile",
  passwdfile => "Path of the /etc/shadow-style password file used",
  pidfile => "Write this pidfile. It is deleted upon server shutdown.",
  "retry-on-error" => "Restart the daemon if any unexpected error happens.",
  help => "Show this help screen.",
);

@opts = qw(stdio|s! log-everything! debug|D! device|d=s baudrate|b=i unix-socket|sock|u=s unix-group|group|g=s logfile|l=s passwdfile|p=s pidfile|P=s retry-on-error! help|h|?);

GetOptions(\%opts,@opts) or usage(2);
if (@ARGV) {
  usage(2);
}

#my $ttyfile = shift || "/dev/ttyUSB0";
#my $initial_baudrate = 0+(shift || 9600);
#my $ux_path = shift || "/tmp/ux_tty_server.sock";
#my $logfile = shift || "/tmp/ux_tty_server.log";

my ($server,$tty,$dev,$stdin,$sel,$running,%input_buffers,$baudrate,$stdout,$dev_bad_input,$dev_last_input,@door_state,$log,$cron);

my %listeners;
my $listener_lifetime = 3600;
my $idle_polltime = 60;
my $dev_idle_timeout = 2*$idle_polltime;
my $idle_awake_cycles;
my $resettable_awake_cycles = 2;


##### log functions #####

sub do_log {
  #my $t = DateTime->now;
  #my $timestamp = "[".$t->ymd." ".$t->hms."] ";
  return unless defined $log;
   my $timestamp = strftime "[%Y-%m-%d %H:%M:%S] ", gmtime;
  print $log $timestamp,@_,"\n";
}

sub log_debug {
  do_log("D: ",@_) if $opts{debug};
  send_listeners("log_debug",join(" ",@_));
}

sub log_notice {
  do_log("N: ",@_);
  send_listeners("log_notice",join(" ",@_));
}

sub log_warning {
  do_log("W: ",@_);
  send_listeners("log_warning",join(" ",@_));
}

sub log_error {
  do_log("E: ",@_);
  send_listeners("log_error",join(" ",@_));
}

sub setup_log {
  if ($logfile ne "") {
    if ($logfile ne "-") {
      my $umask = umask 0077;
      open($log,">>",$logfile) or die "cannot open logfile \"$logfile\": $!";
      umask $umask;
    } else {
      open($log,">&",STDOUT) or die "cannot open stdout as logfile: $!";
    }
    $log->autoflush(1);
  } else {
    undef $log;
  }
  #$stdout = $use_stdio ? \*STDOUT : $log;
}

sub reopen_log {
  log_debug("reopening logfile...");
  setup_log();
  log_debug("logfile opened.");
}

#BEGIN {
#  *Logger::debug = \&log_debug;
#  *Logger::info = \&log_notice;
#  *Logger::warn = \&log_warning;
#  *Logger::error = \&log_error;
#}
#my $logger = bless {}, "Logger";

##### cron functions #####

package Cron {
  # In theory we could use binary heaps to make insert/remove/shift run
  # in O(log(n)) or so, but in reality we don't have that many tasks anyway,
  # so we just lazily use a hash and a sorted list and even just sort the
  # list with "sort" everytime we make an insertion.
  sub new {
    my ($class) = @_;
    return bless {events=>{},table=>[]}, ref $class || $class;
  }

  # schedule by (time,sub) or (time,name,sub);
  # returns event object (may be discarded or used for later deschedule).
  sub schedule {
    my ($self,$time,$name,$sub) = @_;
    my ($events,$table) = @$self{qw(events table)};
    if (@_ <= 2) {
      $sub = $name;# if ref $name eq "CODE";
      $name = "$name";
    }
    #$sub //= $name;
    #$name = "$name";
    die "not a code ref" unless ref $sub eq "CODE";
    my $event = $$events{$name};
    if (defined $event) {
      $event->{time} = $time;
      $event->{sub} = $sub;# if defined $sub;
      @$table = sort {$$a{time} <=> $$b{time}} @$table;
    } else {
      $event = { sub => $sub, time => $time, name => $name };
      $$events{$name} = $event;
      push @$table, $event;
      @$table = sort {$$a{time} <=> $$b{time}} @$table;
    }
    return $event;
  }

  # deschedule by name or event object
  sub deschedule {
    my ($self,$name) = @_;
    my ($events,$table) = @$self{qw(events table)};
    if (ref $name eq "HASH") {
      $name = $name->{name};
    }
    my $event = delete $$events{$name};
    if (defined $event) {
      for (0..$#$table) {
        if ($$table[$_] == $event) {
          splice @$table,$_,1;
          return 1;
          #last;
        }
      }
      #@$table = grep $_ != $event, @$table;
      #for (0..$#$table) {
      #  $$table[$_]{ix} = $_;
      #}
      #if (!defined $event->{ix}) {
        die "cron event found, but not in cron table!";
      #}
      #splice @$table,$event->{ix},1;
      #return 1;
    }
    return 0;
  }

  # get a scheduled event by name. May return undef if the event is not scheduled.
  sub get_event {
    my ($self,$name) = @_;
    my ($events,$table) = @$self{qw(events table)};
    my $event = $$events{$name};
    return $event;
  }

  # check if the given task is already scheduled.
  sub is_scheduled {
    my ($self,$name) = @_;
    if (ref $name eq "HASH") {
      $name = $name->{name};
    }
    my $event = $self->get_event($name);
    return defined $event;
  }

  # peek
  sub get_next_event {
    my ($self) = @_;
    my ($events,$table) = @$self{qw(events table)};
    return $$table[0];
  }

  # peek for the time, may return undef if no events planned.
  # returns $max if otherwise greater or undef.
  sub get_next_time {
    my ($self,$max) = @_;
    my $ev = $self->get_next_event();
    my $time = defined $ev ? $$ev{time} : undef;
    $time = $max if defined $max && (!defined $time || $time > $max);
    return $time;
  }

  # shift the next event from the queue.
  sub shift {
    my ($self) = @_;
    my ($events,$table) = @$self{qw(events table)};
    my $event = CORE::shift @$table;
    if (defined $event) {
      delete $$events{$event->{name}};
    }
    return $event;
  }

  # execute all events that are due.
  # returns the number of events executed.
  sub execute {
    my ($self) = @_;
    my $ev = $self->get_next_event;
    my $i = 0;
    while (defined $ev && $ev->{time} <= time) {
      $self->shift;
      $ev->{sub}($ev);
      $ev = $self->get_next_event;
      $i++;
    }
    return $i;
  }
}

##### rate-limiting functions #####

# at most 2 calls are made per (seconds/hit) time. No delays happen if
# seconds/hit are not reached.
# type => [seconds/hit, acceptable queue len ]
my $rate_limit_config = {
  "socket-pinentry" => [1,10],
};

my $rate_limit_state;
sub rate_limited {
  my ($type,$code) = @_;
  die "not a code ref" unless ref $code eq "CODE";
  my $conf = $rate_limit_config->{$type}//
             $rate_limit_config->{_default}//[1,10];
  my ($dt,$len) = @$conf;
  my $state = \$rate_limit_state->{$type};
  my $time = time;
  if (!defined $$state) {
    $$state = { last => $time-2*$dt, queue => [] };
  }
  if ($$state->{last} <= $time-$dt) {
    $$state->{last} = $time;
    $code->();
  } else {
    my $n = @{$$state->{queue}};
    # we don't want arbitrary queue lengths but we also don't want a DoS by
    # filling up a queue. As a compromise you can fight a DoS by
    # counter-DoSing to get your valid query in between the fake
    # ones by chance.
    my $accept = $n < $len || (rand() < 1/2**($n-$len));
    return 0 unless $accept;
    push @{$$state->{queue}}, $code;
    if ($n == 0) {
      # we don't support changing the config over time, so we just use the
      # locals in the closure.
      my $task;
      $task = sub {
        my $n = @{$$state->{queue}};
        my $code = shift @{$$state->{queue}};
        $cron->schedule(time+$dt,"rate_limit_$type",$task) if $n > 1;
        $code->() if $n > 0;
      };
      $cron->schedule(time+$dt,"rate_limit_$type",$task);
    }
  }
  return 1;
}

##### listener functions #####

sub prune_listeners {
  my $time = time - $listener_lifetime;
  my @old;
  for (keys %listeners) {
    if ($listeners{$_}{time} < $time) {
      push @old, $_;
    }
  }
  delete @listeners{@old} if @old;
}

sub send_listeners {
  my ($type,$content) = @_;
  my $time = time;
  my (@bad,@errs);
  my $datagram = $type." ".$content;
  for (keys %listeners) {
    my $addr = $listeners{$_}{addr};
    my $wants = $listeners{$_}{wants}{$type};
    next unless $wants;
    my $res = undef;
    $res = eval { $server->send($datagram,0,$addr); };
    if (!defined $res) {
      # Error. Remove.
      push @bad, $_;
      push @errs, $@;
    } else {
      $listeners{$_}{time} = $time;
    }
  }
  if (@bad) {
    delete @listeners{@bad};
    log_notice("dropping listener: $_.") for @errs;
  }
}

##### device functions #####

sub send_dev {
  my $buffer = shift;
  print $tty $buffer;
#  if ($buffer =~ /^!b([0-9a-fA-F]*)$/) {
#    $tty->flush;
#    $tty->sync;
#    sleep 1;
#    $baudrate = hex($1);
#    $dev->baudrate($baudrate);
#    sleep 1;
#  }
  send_listeners("W",$buffer);
}

sub schedule_device_ping {
  $cron->schedule(time+$dev_idle_timeout/2,"device_ping",\&do_device_ping);
}

sub set_baudrate {
  my ($baudrate) = @_;
  #$dev->baudrate($baudrate);
  system("stty","-F",$ttyfile,$baudrate) == 0
    or die "cannot stty $ttyfile baudrate to $baudrate";
}

sub setup_device {
  #$tty = do { no warnings "once"; \*GEN0 };
  $baudrate = $initial_baudrate;
  # baudrate, 8 data bits, 1 stop bit, no parity, raw mode, ignore break chars, disable modem control signals, disable on-POSIX special chars, send hangup at close (=DTR-reset the arduino)
  system("stty","-F",$ttyfile,$baudrate,qw(cs8 -cstopb -parenb raw -echo ignbrk clocal -iexten hup)) == 0
    or die "cannot stty $ttyfile to initial settings";
  open($tty,"+<",$ttyfile) or die "cannot open tty $ttyfile: $!";

#  system("stty","-F",$ttyfile,"raw") == 0
#    or die "cannot stty $ttyfile into raw mode";
#  $dev = tie(*$tty,"Device::SerialPort",$ttyfile)
#    or die "cannot open tty $ttyfile: $!";
#  $dev->databits(8);
#  $dev->baudrate($baudrate);
#  #print scalar($dev->baudrate),"\n";
#  $dev->parity("none");
#  $dev->stopbits(1);
#  #$dev->dtr_active(0);

  $tty->blocking(0);
  $dev_bad_input = 0;
  $dev_last_input = time;
  @door_state = (0,0,0,2);
  $idle_awake_cycles = 0;
  schedule_device_ping();
  #$cron->schedule(time+2,"device_init",sub{send_dev("!T\n!d\n")});
  sleep(2); # wait for the device to accept input. After this function returns, we want the device to be ready for commands, so we sleep synchronously.
  # TODO: why does it more than a second to boot?
  send_dev("!T\n!d\n");
}

sub reset_device {
  close($tty);
  #untie $dev;
  setup_device();
}

sub do_device_ping {
  if ($dev_last_input+$dev_idle_timeout < time) {
    log_warning("resetting device (too much inactivity)");
    reset_device();
  }
  send_dev("!0\n");
  schedule_device_ping();
}

##### stdio & server setup functions #####

sub setup_server {
  if (-S $ux_path) {
    unlink $ux_path;
  }
  my $umask = umask 0077;
  $server = IO::Socket::UNIX->new(Type => SOCK_DGRAM, Local => $ux_path, Listen => 5) or die "cannot open server socket: $!";
  umask($umask);
  if ($server_group ne "") {
    #my $gid = (getgrnam($server_group))[2];
    my $gid = getgrnam($server_group);
    die "group $server_group does not exist" unless defined $gid;
    chown -1, $gid, $ux_path and
    chmod 0660, $ux_path or
      log_warning("could not chgrp/chmod server socket for group $server_group/".($gid//"undef"));
    # if we can't chgrp, we keep the socket available to this user only.
  }
}

sub setup_stdio {
  ($stdin,$stdout) = (-1,undef);
  if ($use_stdio) {
    $stdin = \*STDIN;
    # FIXME: This apparently affects *every* process holding that pty:
    $stdin->blocking(0);

    $stdout = \*STDOUT;
    $|=1;
  }
}

sub setup {
  $cron = Cron->new;
  setup_log();
  log_notice("daemon started.");
  setup_server();
  setup_device();
  setup_stdio();
  $running = 0;
  if ($opts{pidfile} ne "") {
    open(my $f,">",$opts{pidfile}) or die "cannot write pidfile: $!";
    print $f $$,"\n";
    close($f);
  }
}

##### input handling: server #####

# some authorization checking may be done here.
# commands are forwarded directly to the device
my $valid_command = qr/^![a-zA-Z0-9].*$/;
# requests are directly processed from this script.
my $valid_request = qr/^\.(?<name>register|unregister|baudrate|close|open|openfor|pinentry|state|reset_device)(?<param>(?: \w+)*)$/;

my @default_wants = qw(W R);

# note that $from may be an empty string if the sender did not bind the socket.
my %request_handlers = (
  register => sub {
    my ($from,$request) = @_;
    return if $from eq "";
    my $param = $request->{param};
    my @types = $param ne "" ? $param =~ /\w+/g : @default_wants;
    my %wants;
    $wants{$_} = 1 for @types;
    $listeners{$from} = { addr => $from, time => time, wants => \%wants };
  },
  unregister => sub {
    my ($from,$request) = @_;
    return unless exists $listeners{$from};
    my $param = $request->{param};
    if ($param ne "") {
      my @types = $param =~ /\w+/g;
      my $wants = $listeners{$from}{wants};
      delete @$wants{@types};
    } else {
      delete $listeners{$from};
    }
  },
  baudrate => sub {
    my ($from,$request) = @_;
    my $param = $request->{param};
    $baudrate = 0+$param;
    send_dev(sprintf "!b%x\n", $baudrate);
    $tty->flush;
    $tty->sync;
    sleep 1;
    set_baudrate($baudrate);
    log_notice("set baudrate to $baudrate");
    sleep 1;
    send_dev("!0\n");
  },
  open => sub {
    my ($from,$request) = @_;
    my $param = $request->{param};
    $param =~ s/^\s*//;
    $param =~ s/\s*$//;
    $param = "unknown" if $param eq "";
    log_notice("open request by $param");
    send_dev("!D1\n");
  },
  openfor => sub {
    my ($from,$request) = @_;
    my $param = $request->{param};
    $param =~ s/^\s*//;
    $param =~ s/\s*$//;
    my ($subsystem, $userid, $check_perm) = split / +/, $param;
    $check_perm //= 1;
    my $valid = 1;
    if ($check_perm) {
      $valid = check_user_permission($userid);
    }
    if ($valid) {
      log_notice("openfor request from $subsystem for $userid (".($check_perm?"valid":"unchecked").")");
      send_dev("!D1\n");
    } else {
      log_notice("openfor request from $subsystem for $userid (invalid)");
    }
    my $datagram = sprintf "openfor %s %d", $userid, $valid?1:0;
    my $res = eval { $server->send($datagram,0,$from); };
  },
  close => sub {
    my ($from,$request) = @_;
    my $param = $request->{param};
    $param =~ s/^\s*//;
    $param =~ s/\s*$//;
    $param = "unknown" if $param eq "";
    log_notice("close request by $param");
    send_dev("!D0\n");
  },
  pinentry => sub {
    my ($from,$request) = @_;
    my $param = $request->{param};
    $param =~ s/^\s*//;
    $param =~ s/\s*$//;
    return if $param =~ /[^0-9]/;
    rate_limited("socket-pinentry",sub{ handle_pinentry($param,"socket"); });
    # handle_pinentry($param,"socket");
  },
  state => sub {
    my ($from,$request) = @_;
    #my $param = $request->{param};
    my $datagram = "state ".join(" ",@door_state);
    my $res = eval { $server->send($datagram,0,$from); };
    if (!$res) {
      log_warning("Could not respond to a state request.");
    }
    return $res;
  },
  reset_device => sub {
    log_notice("resetting device (upon user request)");
    reset_device();
  },
);

# right now, everything is just piped into the device.
# We might want to intercept commands and provide high-level functions.
sub handle_datagram {
  my ($datagram,$from) = @_;
  if ($datagram =~ /$valid_request/) {
    my %request = %+;
    my $req = $request{name};
    $request_handlers{$req}($from,\%request);
  } else {
    my @lines = split /\n/, $datagram;
    for (@lines) {
      return 0 unless /$valid_command/;
      log_notice("valid command: \"$_\"");
    }
    for (@lines) {
      send_dev($_."\n");
    }
  }
}

##### input handling: stdio #####

# right now, everything is just piped into the device.
# We might want to provide management commands.
sub handle_stdin {
  my $buffer = shift;
  send_dev($buffer);
}

##### input handling: device #####

# DONE: relocate user pins to external storage
#my %user_pins = (
#  1234 => "56",
#);

my %special_pins = (
  110 => sub { log_notice("someone called 110"); },
);

# look up crypted password from /etc/shadow-style password file.
# Timing leaks information about number of valid users.
sub passwd_lookup {
  my ($file,$user) = @_;
  open(my $f,"<",$file) or do {
    log_error("cannot open passwd-file \"$file\"");
    return;
  };
  my $res = undef;
  while (my $line = <$f>) {
    next if /^#/;
    my @fields = split /:/, $line, 3;
    next if @fields < 2;
    # we don't want to expose any timing information to attackers, so we don't do shortcuts and loop through the whole file no matter where or whether we find the user entry.
    if ($user eq $fields[0] && !defined $res) {
      $res = $fields[1];
    }
  }
  close($f);
  return $res;
}

# DONE: apply proper authentication with external database and no side channels.
sub verify_pin {
  my ($user,$pin) = @_;
#  my $valid = ($pin eq ($user_pins{$user}//("X" x 12)));
#  return $valid;
#  return 0 == c_verify_pinpad($user,length($user),$pin,length($pin));

# WARNING: Authen::Simple::Passwd is actually a dangerous tool. It sabotages the use of hashed passwords by accepting the hashed password as well. Thankfully we only let the user enter digits and thus are safe from someone entering a hashed password.

# FIXME: file a bug report to upstream about this. Nevermind, there already is. Nobody bothered for THREE YEARS. DONE: reimplement in a sane way.
#  my $passwd = Authen::Simple::Passwd->new(path => $passwd_file, log => $logger);

#  return $passwd->authenticate($user,$pin) ? 1 : 0;

  # again, we don't want to do any shortcuts to prevent leaking information through timing. So we crypt the pin no matter whether we find a valid user.
  # $invalid_crypted is invalid, because the character "_" is not in the set of outputs of sha512-crypt (crypt(*,'$6$'.*.'$')).
  my $invalid_crypted = '$6$xxxxxxxx$xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_';
  my $crypted = passwd_lookup($passwd_file,$user)//$invalid_crypted;
  return crypt($pin,$crypted) eq $crypted ? 1 : 0;
  # TODO: add mechanism for invalidation of often mistyped passwords.
}

sub check_user_permission {
  my $user = shift;
  return 0 unless $user =~ /[0-9]{4}/;
  my $crypted = passwd_lookup($passwd_file,$user);
  return 0 unless defined $crypted;
  # TODO: check optional time-based permission here.
  return 1;
}

sub handle_challenge {
  my ($pin,$source) = @_;
  # we only accept challenge-responses from the physical device.
  return if $source ne "device";
  if ($pin =~ /^(\d{4})(\d{4})(\d+)$/) {
    my ($salt,$user,$pw) = ($1,$2,$3);
    my $valid = verify_pin($user,$pw);
    if ($valid) {
      send_listeners("challenge","$salt $user");
      return 1;
    }
  }
  send_listeners("challenge","FAIL");
  return 0;
}

sub handle_pinentry {
  my ($pin,$source) = @_;
  log_notice("A pin has been entered from $source.");
  my $special = $special_pins{$pin};
  if (defined $special) {
    $special->($pin);
  } elsif ($pin =~ /^(\d{4})(\d+)$/) {
    my ($user,$pw) = ($1,$2);
    if ($user eq "0001") {
      # this is a challenge request.
      handle_challenge($pw,$source);
      return;
    }
    my $valid = verify_pin($user,$pw);
    if ($valid) {
      log_notice("User $user verified by PIN.");
#      if ($door_state[1] == 1) { We don't want to check that state. The door can be panic-locked without the lock sensor noticing. Also, sensors might be faulty?
      send_dev("!m6\n"); # positive melodical feedback
      send_dev("!D1\n");
#      }
    } else {
      log_notice("Wrong password for user $user.");
      send_dev("!m7\n"); # negative melodical feedback
    }
  } else {
    send_dev("!m7\n");
  }
}

# leaving out: "!G%d %d" (!G response), P%d (pinpad debug)
my $valid_devline = qr/^(?:(?<name>!ECHO OFF|OK\.|VERSION 3)|(?<name>PIN|DOOR|AWAKE|SENSE|MFAIL|r[012]|TIME)=(?<param>.*))$/;

my %device_handlers = (
  "!ECHO OFF" => sub {
    log_warning("invalid line sent to device (device complains)");
  },
#  "OK." => sub {},
#  "VERSION 3" => sub {},
  PIN => sub {
    my ($msg) = @_;
    my $pin = $msg->{param};
    $idle_awake_cycles = 0;
    handle_pinentry($pin,"device");
  },
  AWAKE => sub {
    my ($msg) = @_;
    my $param = $msg->{param};
    log_notice("Device ".($param?"awake":"sleeping"));
    if (!$param) {
      $idle_awake_cycles++;
    }
    if ($idle_awake_cycles >= $resettable_awake_cycles) {
      log_warning("resetting device (awake-cycles suggest hardware failure)");
      reset_device();
    }
  },
  DOOR => sub {
    my ($msg) = @_;
    my $param = $msg->{param};
    if ($param =~ /^([01])([01])([012])([012])$/) {
      @door_state = ($1,$2,$3,$4);
      # ($locked,$closed,$mode,$success)
      log_notice("door state: ".
        ("unlocked","locked")[$door_state[0]].", ".
        ("open","closed")[$door_state[1]].", ".
        ("holding","closing","opening")[$door_state[2]].", ".
        ("failed","succeeded","status-info")[$door_state[3]]);
    } else {
      log_warning("invalid door parameter \"$param\"");
    }
  },
# sense is for debugging only.
#  SENSE => sub {
#    my ($msg) = @_;
#    my $param = $msg->{param};
#    if ($param =~ /^ *-?\d+$/) {
#      log_notice("sense is $param");
#    } else {
#      log_warning("invalid sense parameter \"$param\"");
#    }
#  },
  MFAIL => sub {
    my ($msg) = @_;
    my $param = $msg->{param};
    if ($param =~/^[012]$/) {
      log_warning("motor failing: ".("stall","failure to run","failure to stop")[$param]);
    } else {
      log_warning("invalid mfail parameter \"$param\"");
    }
  },
  TIME => sub {
    my ($msg) = @_;
    my $param = $msg->{param};
    my $t = hex($param);
    log_debug("device time is $t");
  },
# r0,r1,r2
);

sub handle_dev {
  my $line = shift;
  send_listeners("R",$line."\n");
  if ($use_stdio) {
    print $stdout $line,"\n";
  }
  if ($log_everything) {
    log_notice("device: ",$line);
  }
  if ($line =~ /$valid_devline/) {
    my %msg = %+;
    my $name = $msg{name};
    my $handler = $device_handlers{$name};
    $handler->(\%msg) if defined $handler;
    $dev_last_input = time;
    $dev_bad_input = 0;
    schedule_device_ping();
  } else {
    $dev_bad_input++;
    log_warning("garbage from device ($dev_bad_input)");
    if ($dev_bad_input > 10) {
      log_warning("resetting device (too much garbage)");
      reset_device();
    }
  }

}

##### idleness handling #####

# nothing to do really, but we might want to act upon idleness by pinging the
# device now and then.
sub handle_idleness {
  prune_listeners();
}

##### main loop #####

sub run {
  my $sel = IO::Select->new(($use_stdio?($stdin):()),$tty,$server);
  $input_buffers{$stdin} //= "";
  $input_buffers{$tty} //= "";

  $running = 1;
  local $SIG{INT} = sub { $running = 0; };
  local $SIG{TERM} = sub { $running = 0; };
  local $SIG{HUP} = \&reopen_log;
  my $ret = 0;

  while ($running) {
    my $time = time;
    my $next_cron = $cron->get_next_time; #($time+$idle_polltime);
    my $polltime = (defined($next_cron) && $next_cron < $time+$idle_polltime) ? $next_cron-$time : $idle_polltime;
    $polltime = 0 if $polltime < 0;
    my @ready = $sel->can_read($polltime);
    my $n_cron = $cron->execute;
    if (!@ready && !$n_cron) {
      handle_idleness();
    }
    for (@ready) {
      my $buffer = "";
      my $buf = \$input_buffers{$_};
      if ($_ == $tty) {
        # Anything coming from the device is copied to stdout as-is.
        # DONE: line-buffer everything and provide it to unix-endpoints.
        # DONE: pull Device::SerialPort out of the loop, since it throws warnings.
        my $res = sysread($tty,$buffer,8192);
        if ($res) {
          $$buf .= $buffer;
          while ($$buf =~ s/^(.*)\n//) {
#          while ($$buf =~ s/^(.*)[\r\n]//) {
            handle_dev($1);
          }
        } elsif (defined $res) {
          log_error("EOF on device");
          $running = 0;
        } else {
          log_error("device disconnected: $!");
        }
      } elsif ($_ == $stdin) {
        # stdin is assumed line-buffered.
        # Piping stuff in here is not a good idea.
        # When stdin is closed, the server terminates.

        my $res = sysread($stdin,$buffer,8192);
        if ($res) {
          $$buf .= $buffer;
          while ($$buf =~ s/^(.*)\n//) {
            handle_stdin($buffer);
          }
        }

#        $buffer = readline($stdin);
#        if (defined $buffer) {
#          $res = length($buffer);
#          handle_stdin($buffer);
#        }
#        if (defined $buffer && $buffer eq "") {
        if (defined($res) && $res == 0) {
           $running = 0;
        }
        if (!defined $res && $! != EAGAIN && $! != EWOULDBLOCK) {
          print STDERR "Error: $!\n";
          log_error("read error: $!");
          #exit 1;
          $ret = 1;
          $running = 0;
        }
      } else {
        # unix datagram;
        # we don't do errors on unix datagrams. It comes in or it doesn't.
        my $from = $_->recv($buffer,8192);
        #print STDERR "<",unpack("H*",$from),">\n";
        handle_datagram($buffer,$from);
        #send_dev($buffer."\n");
      }
    }
  }
  log_notice("exit with code $ret");
  return $ret;
}

while(1) {
  eval {
    setup();
    my $ret = run();
    close($tty); # want to be explicit here.

    if ($opts{pidfile} ne "") {
      unlink $opts{pidfile};
    }
    exit $ret;
  };
  # we're here because the main loop died from an exception.
  # Log the exception and exit or restart.
  if ($@) {
    log_error("Error encountered: \"$@\"");
  }
  exit 1 unless $opts{"retry-on-error"};
  log_notice("Restarting in 10 seconds");
  sleep 10;
}

