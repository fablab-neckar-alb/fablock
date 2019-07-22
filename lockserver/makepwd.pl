#!/usr/bin/perl

# Author: Thomas Kremer
# License: GPL ver. 2 or 3

# usage:
#   ./makepwd.pl user1 user2
#    or
#   cat userlist | ./makepwd.pl
#
#   makes a new random pin for every user and writes it crypted to
#    $shadowfile, uncrypted to $newusersfile and the user-to-id relation
#    to $usersfile.

use strict;
use warnings;
use POSIX qw(floor ceil);

my $usersfile = "users.txt";
my $shadowfile = "pins.shadow";
my $newusersfile = "users.new";

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

sub load_usersfile {
  my $name = shift;
  my %res;
  open (my $f,"<",$name) or die "cannot open \"$name\": $!";
  while(my $line = <$f>) {
    if ($line =~ /^([^:]+): (\d{4}) \d+/) {
      die "duplicate user id in usersfile: $2" if defined $res{$2};
      $res{$2} = $1;
    } else {
      warn "invalid line in usersfile: \"$line\"";
    }
  }
  close($f);
  return \%res;
}

my $pwlen = 8;
#my $saltlen = 8;

my $digits = [0..9];
my $crypt_base64 = [0..9,"a".."z","A".."Z","/","."];
my @users = @ARGV ? @ARGV : <STDIN>;
chomp for @users;
my $usersdb = load_usersfile($usersfile);

open(my $uf, ">>", $usersfile) or die "cannot append to $usersfile: $!";
open(my $sf, ">>", $shadowfile) or die "cannot append to $shadowfile: $!";
open(my $nf, ">>", $newusersfile) or die "cannot append to $newusersfile: $!";

for my $name (@users) {
  my $id;
  do {
    $id = randstr(4,$digits);
  } while defined $$usersdb{$id} || $id =~ /^110/ || $id =~ /^0/;
  # we want 110... and 00... to be reserved for special-use codes.
  # we may want to avoid leading 0s, too. Though it is a little too late for that.
  $$usersdb{$id} = $name;
  my $pw = randstr($pwlen,$digits);

  my $salt = '$6$'. randstr(8,$crypt_base64).'$';
  print $sf "$id:".crypt($pw,$salt).":::::::\n";
  print $uf "$name: $id ".length($pw)."\n";
  print $nf "$name: $id $id$pw\n";
}

