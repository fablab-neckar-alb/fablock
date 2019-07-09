#!/usr/bin/perl

# Author: Thomas Kremer
# License: GPL ver. 2 or 3

# usage:
#   ./makepwd.pl user1 user2 ... > pws.shadow 2>pws.plain
#    or
#   cat userlist | ./makepwd.pl > pws.shadow 2>pws.plain
#
#   makes a new random pin for every user and writes it crypted to
#    stdout and uncrypted to stderr.

use strict;
use warnings;
use POSIX qw(floor ceil);

#randstr 12 0-9 | perl -ne 'if (/^(.{4})(.*)$/) { my $salt = "\$6\$". qx(randstr 8 0-9a-zA-Z/.); chomp $salt; $salt.="\$"; print "$1:".crypt($2,$salt)." -- $1$2\n"; }'

my $randfile = "/dev/urandom";
my $randdev = undef;

sub random {
  my $n = shift;
  # rand() is only weak pseudo-random.
  if (!defined $randdev) {
    open($randdev,"<","$randfile") or die "cannot open $randfile";
  }
  my $bytes = $n <= 256 ? 1 : $n <= 2**16 ? 2 : $n <= 2**32 ? 4 : 8;
  my $max = floor(256.0**$bytes/$n)*$n;
  while(1) {
    my $res = read($randdev,my $buf,$bytes);
    die "cannot read from $randfile: $!" if !defined $res;
    die "short read from $randfile" if $res < $bytes;
    my $r = unpack "Q", $buf."\0"x8;
    if ($r < $max) {
      return $r % $n;
    }
  }
}

sub randstr {
  my ($len,$chars) = @_;
  $len //= 12;
  $chars //= [0..9,"a".."z"];
  return join("",map $$chars[random(scalar(@$chars))],1..$len);
}

my $pwlen = 8;
#my $saltlen = 8;

my $digits = [0..9];
my $crypt_base64 = [0..9,"a".."z","A".."Z","/","."];
my @users = @ARGV ? @ARGV : <STDIN>;
chomp for @users;

for my $name (@users) {
  my $id = randstr(4,$digits);
  my $pw = randstr($pwlen,$digits);

  my $salt = '$6$'. randstr(8,$crypt_base64).'$';
  print STDOUT "$id:".crypt($pw,$salt).":::::::\n";
  print STDERR "$name: $id $id$pw\n";
}

