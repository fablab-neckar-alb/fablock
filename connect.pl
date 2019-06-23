#!/usr/bin/perl

use strict;
use warnings;

use IO::Handle;
use IO::Select;
use POSIX qw(EAGAIN EWOULDBLOCK);
use Device::SerialPort;

my $ttyfile = shift || "/dev/ttyUSB0";
my $baudrate = shift || 9600; # UNUSED for now.

my $tty = do { no warnings "once"; \*GEN0 };
system("stty","-F",$ttyfile,"raw") == 0 or die "cannot stty $ttyfile into raw mode";
my $dev = tie(*$tty,"Device::SerialPort",$ttyfile) or die "cannot open tty $ttyfile: $!";
$dev->databits(8);
$dev->baudrate($baudrate);
#print scalar($dev->baudrate),"\n";
$dev->parity("none");
$dev->stopbits(1);
#$dev->dtr_active(0);

# open(my $tty, "+<", $ttyfile) or die "cannot open tty $ttyfile: $!";
my $stdin = \*STDIN;
$stdin->blocking(0);
$tty->blocking(0);
$|=1;

my $sel = IO::Select->new($stdin,$tty);

my $running = 1;
$SIG{INT} = sub { $running = 0; };

while ($running) {
  my @ready = $sel->can_read(1000);
  for (@ready) {
    my $buffer = "";
    my $res;
    if ($_ == $tty) {
      $res = sysread($tty,$buffer,4096);
      #$buffer =~ s/(.)/ sprintf "\e[34m%02x\e[01;32m%s\e[0m",ord($1),($1 ge " " && $1 le "\x7f" || $1 eq "\n") ? $1 : ".";/ges;
#      $buffer =~ s/(.)/ sprintf "%02x",ord($1);/ges;
      print STDOUT $buffer;
    } else {
      #$res = sysread($stdin,$buffer,4096);
      $buffer = readline($stdin);
      if (defined $buffer) {
        $res = length($buffer);
        print $tty $buffer;
        if ($buffer =~ /^!b([0-9a-fA-F]*)$/) {
          $tty->flush;
          $tty->sync;
          sleep 1;
          $baudrate = hex($1);
          $dev->baudrate($baudrate);
          sleep 1;
        }
      }
      #if (defined($res) && $res == 0) {
      if (defined $buffer && $buffer eq "") {
        $running = 0;
      }
    }
    if (!defined $res && $! != EAGAIN && $! != EWOULDBLOCK) {
      print STDERR "Error: $!\n";
      exit 1;
    }
  }
}

close($tty);

