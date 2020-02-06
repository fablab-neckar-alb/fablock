#!/usr/bin/perl

# play a nice audio to invite people to enter the lab after unlocking.

use strict;
use warnings;

use IO::Select;
use IO::Socket::UNIX;

$ENV{ALSA_CARD} = "USB";

sub play_sound {
  my $name = shift;
  print STDERR "<$name>\n";
  my $file = "lockserver-audio/$name.wav";
  return unless -e $file;
  my $pid = fork();
  if (!$pid) {
    if (defined $pid) {
      exec("aplay","-q",$file) or die "cannot exec";
    } else {
      system("aplay","-q",$file);
    }
  }
}

my $ux_path = "/run/lockserver.sock";

my $sock = IO::Socket::UNIX->new(Type => SOCK_DGRAM, Local => "", Peer => $ux_path);

$sock->send(".register log_notice");
my $register_time = time+1800;

my $buffer = "";
my $running = 1;

$SIG{INT} = sub { $running = 0; };
$SIG{CHLD} = "IGNORE";

my $sel = IO::Select->new($sock);

my $state = 0; # $state == 1 iff we want the door to be unlocked.
my @lockstate = ("","","","");

while ($running) {
  my $delta = $register_time-time;
  if ($delta <= 0) {
    $sock->send(".register log_notice");
    $register_time = time+1800;
    $delta = 1800;
  }
  if ($sel->can_read($delta+10)) {
    my $from = $sock->recv($buffer,8192);
    # log_notice door state: unlocked, closed, holding, status-info
    #print STDERR "<$buffer>\n";
    if ($buffer =~ /^log_notice door state: ([^,]+), ([^,]+), ([^,]+), ([^,]+)$/) {
      my @newlockstate =
             ($1 eq "locked"?1:0,$2 eq "closed"?1:0,$3,$4);
      my ($locked,$closed,$action,$success) = @newlockstate;
      my ($prev_locked,$prev_closed,$prev_action,$prev_success) = @lockstate;
      #my @lockchanged = map $lockstate[$_] ne $newlockstate[$_], 0..3;
      if ($action eq "opening") {
        if ($success eq "succeeded") {
          play_sound("enter");
        } elsif ($success eq "failed") {
          play_sound("open_failed");
        }
        $state = 1;
      } elsif ($action eq "closing") {
        if ($success eq "succeeded") {
          play_sound("lock_succeeded");
        } elsif ($success eq "failed") {
          play_sound("lock_failed");
        }
        $state = 0;
      } elsif ($action eq "holding" && $success eq "status-info") {
        if (!$locked && !$closed) {
          if ($state == 0) {
            play_sound("bye");
          } else {
            play_sound("greetings");
          }
        } elsif (!$locked && $closed && !$prev_closed) {
          if ($state == 0) {
            play_sound("post_bye");
          } else {
            play_sound("post_greetings");
          }
        }
      }
      @lockstate = @newlockstate;
    } elsif ($buffer =~ /^log_notice Wrong password for user \S+\.$/) {
      play_sound("access_denied");
    } elsif ($buffer =~ /^log_notice User \S+ verified by PIN\.$/) {
      play_sound("access_granted");
    }
  }
#  /A pin has been entered\./
#  /N: User \d+ verified by PIN./
}

