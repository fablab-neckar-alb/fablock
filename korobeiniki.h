/*
  "Korobeiniki" is a 19th-century traditional russian folk song. Wikipedia says it's in the public domain. There is a trademark in the video game business though.
*/

//melody_begin(korobeiniki_a,240)
melody_begin(korobeiniki_a,2*151)
  melody_note(e,1,2)
  melody_note(b,0,1)
  melody_note(c,1,1)

  melody_note(d,1,2)
  melody_note(c,1,1)
  melody_note(b,0,1)

  melody_note(a,0,2)
  melody_note(a,0,1)
  melody_note(c,1,1)

  melody_note(e,1,2)
  melody_note(d,1,1)
  melody_note(c,1,1)

  melody_note(b,0,3)
  melody_note(c,1,1)

  melody_note(d,1,2)
  melody_note(e,1,2)

  melody_note(c,1,2)
  melody_note(a,0,2)

  melody_note(a,0,2)
  melody_rest(2)

  melody_rest(1)
  melody_note(d,1,2)
  melody_note(f,1,1)

  melody_note(a,1,2)
  melody_note(g,1,1)
  melody_note(f,1,1)

  melody_note(e,1,3)
  melody_note(c,1,1)

  melody_note(e,1,2)
  melody_note(d,1,1)
  melody_note(c,1,1)

  melody_note(b,0,2)
  melody_note(b,0,1)
  melody_note(c,1,1)

  melody_note(d,1,2)
  melody_note(e,1,2)

  melody_note(c,1,2)
  melody_note(a,0,2)

  melody_note(a,0,2)
  melody_rest(2)
melody_end()

//melody_begin(korobeiniki_b,240)
melody_begin(korobeiniki_b,2*151)
  melody_note(e,0,4)
  melody_note(c,0,4)
  melody_note(d,0,4)
  melody_note(b,-1,4)

  melody_note(c,0,4)
  melody_note(a,-1,4)
  melody_note(gis,-1,6)
  melody_rest(2)

  melody_note(e,0,4)
  melody_note(c,0,4)
  melody_note(d,0,4)
  melody_note(b,-1,4)

  melody_note(c,0,2)
  melody_note(e,0,2)
  melody_note(a,0,2)
  melody_note(a,0,2)
  melody_note(gis,0,6)
  melody_rest(2)
melody_end()

melody_t *const korobeiniki_progression[] PROGMEM = {
  &korobeiniki_a,
  &korobeiniki_a,
  &korobeiniki_b,
  NULL
};

