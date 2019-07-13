#!/usr/bin/perl

# Copyright (c) 2019 by Thomas Kremer
# License: GNU GPL ver. 2 or 3

# usage: midi2melody.pl some/midifile.mid [<cname>|] [<unit>] > c_header.h
#     converts a midi file to a C header, defining <cname> as a melody, using
#     <unit>th notes as a base metric.
#     The C header is written to standard output.
#     If <cname> is not given or an empty string, it is derived from
#     the midi filename. <cname> may be ".gcode" to output G-Code instead.

# TODO: commandline selector for which track to extract.
# DONE: adapt for gcode: "M300 S$freq P$msec"

use strict;
use warnings;
#use Data::Dumper;
use MIDI;

my $fname = shift;
my $cname = shift;
my $unit = shift//32; # 16th note.

if (defined $cname && $cname ne "") {
  if ($cname =~ /\W/ || $cname =~ /^\d/) {
    print STDERR "invalid cname \"$cname\"!\n";
  }
} else {
  $cname = lc($fname);
  $cname =~ s{^.*/}{};
  $cname =~ s/\.[^\.]*$//;
  $cname =~ s/ /_/g;
  $cname =~ s/\W//g;
  $cname =~ s/^(?=\d)/_/;
  $cname = "melody" if $cname eq "";
}

my $opus = MIDI::Opus->new({from_file => $fname});
my $ticks = $opus->ticks;
my $ticks_u = $ticks*4/$unit;
my @tracks = grep $_->type eq "MTrk", $opus->tracks;

my @tempos;
for (@tracks) {
  $_ = [$_->events];
  my $name;
  my $t = 0;
  my $last = undef;
  my $t_int_last = 0;
  for (@$_) {
    $_ = [@$_];
    $_->[1] /= $ticks_u;
    $t += $_->[1];
    my $t_int = int($t);
    my $h = {
      "track_name" => sub { $name = $_->[2]; },
      "set_tempo" => sub { push @tempos, [$t, $_->[2]]; },
      "note_on" => sub {
#         if ($_->[4] == 0) {
#           $_->[0] = "rest";
#           if (defined $last && $last->[0] eq "note_on" && $last->[3] == $_->[3]) {
#             $last->[5] = $_->[1];
#           }
#         }
         #$last->[5] = $_->[1] if defined $last;
         $last->[5] = ($t_int-$t_int_last) if defined $last;
         },
    }->{$_->[0]};
    $h->() if defined $h;
    $last = $_;
    $t_int_last = $t_int;
  }
  my $len = $t;
  print STDERR "Track name: ".($name//"undef")."; Length: ".$len.";\n";
  my $cnotes = "";
  my $notes = [];
  for (@$_) {
    if ($_->[0] eq "note_on" && ($_->[5]//0) != 0) {
      #print STDERR join(",", @$_),"\n";
      $cnotes .= $$_[4] == 0 ? sprintf("  melody_rest(%d)\n", $$_[5]) :
                   sprintf("  melody_midi(%d,%d)\n", $$_[3], $$_[5]);
      push @$notes, $$_[4] == 0 ? [0,$$_[5]] : [$$_[3],$$_[5]];
    }
  }
  $_ = { cnotes => $cnotes, len => $len, name => $name, notes => $notes };
}

if (@tempos > 1 || (@tempos == 1 && $tempos[0][0] != 0)) {
  print STDERR "using first tempo explicitly given, since there are more:\n";
  for (@tempos) {
    print STDERR "  at time $$_[0]: tempo $$_[1]us/4th; bpm = ". $unit/4*60000000/$$_[1]."\n";
  }
}
# default is 120bpm for quarters.
my $bpm = @tempos ? ($unit/4*60000000/$tempos[0][1]) : 120*($unit/4);

@tracks = grep $_->{len} != 0, @tracks;

my $i = 1;
my $cnamei = $cname;
for (@tracks) {
  if ($cname eq ".gcode") {
    my $namecomment = @tracks != 1 ? ", name: ".(defined($_->{name})?"\"$_->{name}\"":"none") : "";
    print "; track $i ($bpm bpm)$namecomment\n";
    for (@{$_->{notes}}) {
      my ($pitch,$len) = @$_;
      my $msec = $len*60000/$bpm;
      if ($pitch == 0) {
        printf "G4 P%d\n", $msec;
      } else {
        my $freq = 440*2**(($pitch-69)/12);
        printf "M300 S%d P%d\n", $freq, $msec;
      }
    }
  } else {
    my $namecomment = @tracks != 1 ? " // name: ".(defined($_->{name})?"\"$_->{name}\"":"none") : "";
    print "melody_begin($cnamei,$bpm)$namecomment\n";
    print $_->{cnotes};
    print "melody_end()\n";
    print "\n";
  }
  $i++;
  $cnamei = $cname.$i;
}


#print "Tempos: ".join(",",map join(",",@$_), @tempos)."\n";

#print "ticks = ".$opus->ticks."; Tracks: ".join("\n",map "[".join(",",map "[".join(",",@$_)."]", $_->events)."]",$opus->tracks)."\n";'


