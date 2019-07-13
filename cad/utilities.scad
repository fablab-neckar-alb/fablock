/*
  Copyright (c) 2015-2017 by Thomas Kremer
  License: GNU GPL ver. 2 or 3
*/

no_virtuals = false;

// === fitting and friction avoiding constants ===

/*
  tests suggest the following:
  - cylinders of radii < 1 are fragile and easily broken.
  - the printer seems to constantly print 0.05mm more than specified, sometimes 0.1, sometimes even 0.0. (v=70mm/s, dz=0.2mm, T=220 deg, yellow filament TODO: diameter?)
  - consequently, holes are actually 2x0.05mm too small.
  - This also applies to vertical holes.
  - vertical axes in vertical holes are easily broken in assembly due to
     layered structure. But go smoothly.
  - 0.05 does no good. Maybe for hammering nails...
  - 0.1 sticks. Still need a hammer.
  - 0.15 sticks. Horizontal sliding possible. Vertical sliding sometimes difficult.
  - 0.2 fits and slides. Vertical sliding still a little rough.
  - 0.25 slides better.
  - gears go fine without backlash.
  - rafts are between a pain and impossible to remove.
  - small supports are also a pain to remove.
  - consider printing sliding gear axes horizontally (separately if necessary).
  - floating not measured. Consider one print layer (->0.2).
  - resting not measured. Consider half a print layer (->0.1) or postprocessing.
  - actually, printing 1mm radius holes stick well on 1mm radius real motors.
*/
rest_backlash  = 0.1;  // for accurately stacking objects onto each other
stick_backlash = 0.15; // for firmly sticking objects into holes
fit_backlash   = 0.25;  // for removably fitting objects into holes
slide_backlash = 0.3; // for objects gliding along other objects
float_backlash = 0.35;  // minimum distance for avoiding friction
gear_backlash  = 0.05;  // backlash for gears toward each other

/*
rest_backlash  = 0.1;  // for accurately stacking objects onto each other
stick_backlash = 0.15; // for firmly sticking objects into holes
fit_backlash   = 0.2;  // for removably fitting objects into holes
slide_backlash = 0.25; // for objects gliding along other objects
float_backlash = 0.3;  // minimum distance for avoiding friction
gear_backlash  = 0.0;  // backlash for gears toward each other
*/

amlaser_radius = 0.095; // radius of laser beam of laser cutter


// === for our external build system (used for e.g. splinecyl) ===

// the external build system will override this variable. To have code that
// checks it work outside of the build system, we need to initialize it.
currently_built_part = undef;
currently_building = currently_built_part != undef;

module part(name) {
  if (currently_built_part == name)
    children();
}

module demo() {
  if (!currently_building)
    children();
}

// === helper functions and constants ===

pi=3.1415926535897932384626433832795;
mat_2d_to_3d = [[1,0],[0,1],[0,0]];

function turn_matrix(v=[0,0,0],x=false,y=false,z=false) =
 x!=false?[[1,0,0],[0,cos(x),-sin(x)],[0,sin(x),cos(x)]]:
 y!=false?[[cos(y),0,sin(y)],[0,1,0],[-sin(y),0,cos(y)]]:
 z!=false?[[cos(z),-sin(z),0],[sin(z),cos(z),0],[0,0,1]]:
 turn_matrix(z=v[2])*
 turn_matrix(y=v[1])*
 turn_matrix(x=v[0]);

/*
    ^ w'
  -_ \  ^ v
    -_\/      
   <--+-_     
   w     -_
  w = m*v + n*w2, w2*v = 0
  w' = -m*v + n*w2 = w-2*m*v
  m*v = (v*w)/(|v|^2)*v = (v*w)/(v*v)*v
  w' = w-2*(v*w)/(v*v)*v
  M = id - v*v^t*2/(v*v) = id - [[v[0]],[v[1]],[v[2]]]*[v]*2/(v*v)
*/
function mirror_matrix(v=[0,0,1]) =
    [[1,0,0],[0,1,0],[0,0,1]] - [[v[0]],[v[1]],[v[2]]]*[v]*2/(v*v);

// normal,line vector, line source point (plane is in center)
// result is the intersection point.
// x: x*n = 0, x-p = m*v;
// (p+m*v)*n = 0; p*n + m*(v*n) = 0; m = -(p*n)/(v*n); (v*n != 0)
function plane_intersect(n,v,p) = p-(p*n)/(v*n)*v;

/*
  intersection of two circles:

 (x-gx_a)^2+y^2 = gsh_b^2, (x-g1_pos[0])^2+(y-g1_pos[1])^2 = gsh_a^2

  (x-x0)^2+(y-y0)^2 = r0^2,
  (x-x1)^2+(y-y1)^2 = r1^2

  (x-x0)^2-(x-x1)^2+(y-y0)^2-(y-y1)^2 = r0^2-r1^2,
  x0^2-x1^2-2x(x0-x1)+y0^2-y1^2-2y(y0-y1) = r0^2-r1^2,
  2x(x0-x1)+2y(y0-y1) = -r0^2+r1^2+x0^2+y0^2-x1^2-y1^2 = 2*C,
  y = (C-x(x0-x1))/(y0-y1),

  (x-x0)^2+((C-x(x0-x1))/(y0-y1)-y0)^2 = r0^2,
  x^2-2*x*x0+x0^2+(C^2-2Cx(x0-x1)+x^2(x0-x1)^2)/(y0-y1)^2-2*(C-x(x0-x1))/(y0-y1)*y0 + y0^2 = r0^2,
  x^2*(y0-y1)^2-2*x*x0*(y0-y1)^2+C^2-2Cx(x0-x1)+x^2(x0-x1)^2-2*(C-x(x0-x1))*(y0-y1)*y0 + (x0^2+y0^2-r0^2)*(y0-y1)^2 = 0,
  x^2*(y0-y1)^2 -2*x*x0*(y0-y1)^2 +C^2 -2Cx(x0-x1) +x^2(x0-x1)^2 -2*C*(y0-y1)*y0 +2*x(x0-x1)*(y0-y1)*y0 + (x0^2+y0^2-r0^2)*(y0-y1)^2 = 0,
  x^2*(y0-y1)^2 +x^2(x0-x1)^2 -2*x*x0*(y0-y1)^2 -2Cx(x0-x1) +2*x(x0-x1)*(y0-y1)*y0  +C^2 -2*C*(y0-y1)*y0 +(x0^2+y0^2-r0^2)*(y0-y1)^2 = 0,
  x^2*((y0-y1)^2+(x0-x1)^2) +x*(-2*x0*(y0-y1)^2 -2C(x0-x1) +2*(x0-x1)*(y0-y1)*y0)  +C^2 -2*C*(y0-y1)*y0 +(x0^2+y0^2-r0^2)*(y0-y1)^2 = 0,

  a = ((y0-y1)^2+(x0-x1)^2)
  b = 2*(-x0*(y0-y1)^2 -C(x0-x1) +(x0-x1)*(y0-y1)*y0)
  c = C^2 -2*C*(y0-y1)*y0 +(x0^2+y0^2-r0^2)*(y0-y1)^2,

  x = -b+-sqrt(b^2-4ac)/(2*a)

*/

function quadratic_solutions(a,b,c) =
  [(-b+sqrt(b*b-4*a*c))/(2*a),(-b-sqrt(b*b-4*a*c))/(2*a)];

function circles_intersections_helper2(x0,y0,r0,x1,y1,r1,C,x) =
  [[x[0],(C-x[0]*(x0-x1))/(y0-y1)],[x[1],(C-x[1]*(x0-x1))/(y0-y1)]];

function circles_intersections_helper(x0,y0,r0,x1,y1,r1,C) =
  circles_intersections_helper2(x0,y0,r0,x1,y1,r1,C,
    x = quadratic_solutions(
      a = (pow(y0-y1,2)+pow(x0-x1,2)),
      b = 2*(-x0*pow(y0-y1,2) -C*(x0-x1) +(x0-x1)*(y0-y1)*y0),
      c = pow(C,2) -2*C*(y0-y1)*y0 +(pow(x0,2)+pow(y0,2)-pow(r0,2))*pow(y0-y1,2)
    )
  );
function circles_intersections(p1,r1,p2,r2) =
  circles_intersections_helper(p1[0],p1[1],r1,p2[0],p2[1],r2,
    C = (p1*p1-p2*p2-r1*r1+r2*r2)/2
  );


function turn_to_vector(v,v3=false,v10=false,v11=false,v1=false,v2=false) =
  v3 == false ? turn_to_vector(v,v/norm(v)) :
  v10 == false ? turn_to_vector(v,v3,cross(v3,[0,0,1])) :
  v11 == false ? turn_to_vector(v,v3,v10,norm(v10) < 0.5 ? cross(v3,[0,1,0]) : v10) :
  v1 == false ? turn_to_vector(v,v3,v10,v11,v11/norm(v11)) :
  v2 == false ? turn_to_vector(v,v3,v10,v11,v1,cross(v1,v3)) :
    [[v1[0],v2[0],v3[0],0],
     [v1[1],v2[1],v3[1],0],
     [v1[2],v2[2],v3[2],0],
     [0,0,0,1]];


// === spline stuff ===

module cyl(p1=[0,0,0],p2=false,r=1,r1=false,r2=false,$fn=$fn) {
  two_point = p2!=false;
  p2 = two_point?p2-p1:p1;
  p1 = two_point?p1:[0,0,0];
  r1 = r1!=false?r1:r;
  r2 = r2!=false?r2:r;
  h = norm(p2);

  v3 = p2/norm(p2);
  v10 = cross(v3,[0,0,1]);
  v11 = norm(v10) < 0.5 ? cross(v3,[0,1,0]) : v10; // norm==sin(v3,e3)
  // norm at least 1/2 now (could be squeezed to 1/sqrt(2))
  v1 = v11/norm(v11);
  v2 = cross(v1,v3);

  // needs to map e3 to p2 and be orthogonal.
  mat = [[v1[0],v2[0],v3[0],0],
         [v1[1],v2[1],v3[1],0],
         [v1[2],v2[2],v3[2],0],
         [0,0,0,1]];

  translate(p1)
    multmatrix(mat)
      cylinder(r1=r1,r2=r2,h=h,$fn=$fn);
}

// c is an array of 4 control points, 0 <= t <= 1. dc and d2c are internal.
function bezier_eval(c,t,dc=false,d2c=false) = 
  dc == false ? bezier_eval(c,t,
    [c[0]*(1-t)+c[1]*t,c[1]*(1-t)+c[2]*t,c[2]*(1-t)+c[3]*t]) :
  d2c == false ? bezier_eval(c,t,dc,
    [dc[0]*(1-t)+dc[1]*t,dc[1]*(1-t)+dc[2]*t]) :
  d2c[0]*(1-t)+d2c[1]*t;  

// spline is an array of 3*N+1 control points, t arbitrary. ix is internal.
function bezier_spline_eval(spline,t,ix=false) = 
  ix == false ?
    t <= 0 ? spline[0] :
    t >= floor((len(spline)-1)/3) ? spline[len(spline)-1] :
    bezier_spline_eval(spline,t-floor(t),floor(t)*3) :
  bezier_eval([spline[ix],spline[ix+1],spline[ix+2],spline[ix+3]],t);

// cylinder from p1 to p2, cut to fit cylinders from p0 to p1 and p2 to p3.
// needed for polycyl and splinecyl.
// In extreme cases, a sphere part is injected. (extreme = angles above 20 deg)
module polycyl_part(p0,p1,p2,p3,r) {
  /*
     ______/    ______.
     _____/\    ___/-^
         /\ \     / /
           \     / /

    FIXED: a lot of nans.
  */
  r0 = len(p1) >= 4 ? p1[3]*r : r;
  r1 = len(p2) >= 4 ? p2[3]*r : r;
  p03 = [p0[0],p0[1],p0[2]];
  p13 = [p1[0],p1[1],p1[2]];
  p23 = [p2[0],p2[1],p2[2]];
  p33 = [p3[0],p3[1],p3[2]];
  p1 = p13;
  p2 = p23;
  at_start = p0 == false;
  at_end   = p3 == false;
  p0 = (p0 == false || p03 == p13) ? p1*2-p2 : p03;
  p3 = (p3 == false || p33 == p23) ? p2*2-p1 : p33;
  dp0 = p1-p0;
  dp  = p2-p1;
  dp1 = p3-p2;
  c0 = (dp0*dp)/(norm(dp0)*norm(dp));
  c1 = (dp*dp1)/(norm(dp)*norm(dp1));
  cosa = c0 > 0.99 ? 1 : c0;
  cosb = c1 > 0.99 ? 1 : c1;
  ds0 = min(norm(dp),norm(dp0))/4;
  ds1 = min(norm(dp),norm(dp1))/4;
/*
  a2 = acos(cosa)/2;
  b2 = acos(cosb)/2;
*/
  v0 = cross(dp0,dp)/(norm(dp0)*norm(dp));
  v1 = cross(dp,dp1)/(norm(dp)*norm(dp1));
  sina = norm(v0);
  sinb = norm(v1);
  a = cosa > 0.5 ? asin(sina) : acos(cosa);
  b = cosb > 0.5 ? asin(sinb) : acos(cosb);
/*
  a2 = asin(sina)/2;
  b2 = asin(sinb)/2;
*/
  a2 = a/2;
  b2 = b/2;

  // The sphere version is always "nicer", but when we want infinitesimal
  // joints, it's expected to be a performance killer. So we just call
  // 20 degrees infinitesimal, at that point the difference is not really
  // visible anymore.
  //use_sphere0 = a > 20;
  //use_sphere1 = b > 20;

  // However, STL export seems to choke on the no-sphere version ("Object isn't a valid 2-manifold!"), so we go back to always-sphere. (except end-pieces)
  use_sphere0 = !at_start;
  use_sphere1 = !at_end;
  n0 = dp0/norm(dp0)+dp/norm(dp);
  n1 = dp1/norm(dp1)+dp/norm(dp);
  h0 = use_sphere0 ? 0 : min(tan(a2),10);
  h1 = use_sphere1 ? 0 : min(tan(b2),10);
  R0 = a2 < 89 ? 1/cos(a2) : 10;
  R1 = b2 < 89 ? 1/cos(b2) : 10;
  r00 = max(r0-(r1-r0)*r0*h0/norm(dp),0);
  r10 = max(r1+(r1-r0)*r1*h1/norm(dp),0);
//  echo(str("(h0,h1)=",[h0,h1],";(p1,p2)=",[p1,p2],";(a2,b2)=",[a2,b2],"."));
  difference() {
    union() {
      translate(p1)
        multmatrix(turn_to_vector(dp))
          translate([0,0,-r0*h0])
            cylinder(r1=r00,r2=r10,h=norm(dp)+r0*h0+r1*h1);
      if (use_sphere0) {
//        color([1,0,0])
        translate(p1)
          difference() {
            multmatrix(turn_to_vector(n0))
              sphere(r0);
            multmatrix(turn_to_vector(dp))
              translate([0,0,ds0])
                cylinder(r=r0+0.1,h=2*r0);
            multmatrix(turn_to_vector(-n0))
              translate([0,0,ds0])
                cylinder(r=r0+0.1,h=2*r0);
          }
//        echo("sphere0");
      }
      if (use_sphere1) {
//        color([1,0,0])
        translate(p2)
          difference() {
            multmatrix(turn_to_vector(n1))
              sphere(r1);
            multmatrix(turn_to_vector(-dp))
              translate([0,0,ds1])
                cylinder(r=r1+0.1,h=2*r1);
            multmatrix(turn_to_vector(n1))
              translate([0,0,ds1])
                cylinder(r=r1+0.1,h=2*r1);
          }
//        echo("sphere1");
      }
    }
    if (!use_sphere0)
      translate(p1)
        multmatrix(turn_to_vector(-n0))
          cylinder(r=max(r0,r00)*(R0+h0)+0.1,h=max(r0,r00)*sqrt(pow(h0,2)+1)+0.1);
    if (!use_sphere1)
      translate(p2)
        multmatrix(turn_to_vector(n1))
          cylinder(r=max(r1,r10)*(R1+h1)+0.1,h=max(r1,r10)*sqrt(pow(h1,2)+1)+0.1);

  }
}

// make a cylinder between each neighboring points in the list, fitting well
// together. Points may contain a fourth coordinate denoting a radius factor
// for that point.
module polycyl(points=[],r=1) {
  union()
    for (i=[0:len(points)-2])
      polycyl_part(i > 0 ? points[i-1]:false,points[i],points[i+1],i < len(points)-2 ? points[i+2]:false,r);
}

// make a circle at each point, to be connected into a deformed
// cylinder (!closed) or torus (closed).
// i'th circle's normal is at diff[i] (default from points[i+1]-points[i-1]).
// and y[i] has to be given and linearly independent from diff[i] if the curvature is not continuously non-null.
// The radius of the circle is r. TODO: allow variable radius r[i].
// shape can be given to use a non-circular shape instead.
// The caller has to ensure that the shape given does not self-intersect.
module polycyl_v2(points=[],diff=false,y=false,r=1,$fn=$fn,shape=false,closed=false,convexity=2) {
  $fn = shape ? len(shape) : $fn ? $fn : 8;
  shape = shape ? shape : [ for (i=[0:$fn-1]) [cos(i*360/$fn),sin(i*360/$fn)] ];
  //L = [[1,0.1],[0.1,0.1],[0.1,1],[-0.5,1],[-0.5,-0.5],[1,-0.5]];
  //$fn = len(L);
  function circle_p(fn=$fn,p=[0,0,0],x=[1,0,0],y=[0,1,0],i) =
    p+x*shape[i][0]+y*shape[i][1];
  //function circle_p(fn=$fn,p=[0,0,0],x=[1,0,0],y=[0,1,0],i) =
  //  p+x*cos(i*360/fn)+y*sin(i*360/fn);

  N = len(points);

  function n0(x) = norm(x)!=0?x/norm(x):x;

  diff = diff ? diff : [ for (i=[0:N-1]) closed ?
                                          (points[(i+1)%N]-points[(i+N-1)%N])/2 :
                                         i > 0 ? i < N-1 ?
                                          (points[i+1]-points[i-1])/2 :
                                          points[i]-points[i-1] :
                                          points[i+1]-points[i] ];

  for (j=[0,N-1])
    echo(j,diff[j],n0(points[(j+1)%N]-points[j])+n0(points[(j+N-1)%N]-points[j]),      cross(diff[j],n0(points[(j+1)%N]-points[j])+n0(points[(j+N-1)%N]-points[j])));
  y = y&&len(y[0]) ? y :
      y?[ for (i=[0:N-1]) y ] :
        [ for (j=[0:N-1])
            let (i = j==0?1:j==N-1?N-2:j,
              v1=closed ?
                   n0(points[(j+1)%N]-points[j])+n0(points[(j+N-1)%N]-points[j]) :
                   n0(points[i+1]-points[i])+n0(points[i-1]-points[i]),
// not as concise as p+r-2*q, but numerically more sane
// (unless it's optimized out)
              v2=[0,0,1]) //,v3=[0,1,0])
                norm(v1)>0 ? v1 : v2 ];
                // norm(cross(v2,diff[j]))>0.2*norm(diff[j])?v2:v3 ];

  function sp_p(i) = points[i];
  function sp_z(i) = diff[i];
  function sp_x(i) = cross(y[i],sp_z(i)); // FIXME: kill nulls.
  function sp_y(i) = cross(sp_x(i),sp_z(i));
  //function circle(fn=$fn,p=[0,0,0],x=[1,0,0],y=[0,1,0]) =
  //  [ for (i=[0:fn-1]) circle_p(fn,p,x,y,i) ];

  //circle_count = N + (closed ? 0 : 1);
  circle_count = N;
  vertices = [ for (j=[0:circle_count-1],i=[0:$fn-1])
                   circle_p($fn,sp_p(j),r*sp_x(j)/norm(sp_x(j)),r*sp_y(j)/norm(sp_y(j)),i) ];
  outerfaces = [ for (j=[0:N-1-(closed?0:1)],i=[0:$fn-1],
                      p=[[j*$fn+i,j*$fn+(i+1)%$fn,((j+1)%circle_count)*$fn+i],
                         [((j+1)%circle_count)*$fn+(i+1)%$fn,((j+1)%circle_count)*$fn+i,j*$fn+(i+1)%$fn]]
                     ) p ];
  last = (circle_count-1)*$fn;
  faces = closed ? 
    outerfaces
    :
    concat([ for (i=[1:$fn-2]) [0,i+1,i]], outerfaces,
           [ for (i=[1:$fn-2]) [last,last+i,last+i+1]]);

  polyhedron(points=vertices,faces=faces,convexity=convexity);
}


// make a cylindrical shape along a spline. The spline may contain a fourth
// coordinate denoting a radius factor for each point on the spline.
// Each bezier line is evaluated at <slices> points.
module splinecyl(spline=[],r=1,$fn=$fn,slices=max($fn,2),extern=false,closed=false) {
  N = floor((len(spline)-1)/3);
  dt = 1/slices;
  // we have a build system that allows to create objects like spline
  // cylinders externally, just dumping the name for the external utility
  // to get the arguments:
  t0 = 0;
  t1 = N;
  if (extern != false) {
    echo(str("extern module ",extern,": r",r,":n",slices,":fn",$fn,":closed",closed?1:0,":",spline));
  }
  union()
    for (i=[0:slices*N-1],t=[i*dt]) {
/*      echo(str("t=",t,",i=",i,",N=",N,",dt=",dt,";c=[",
        [bezier_spline_eval(spline,t-dt),
        bezier_spline_eval(spline,t),
        bezier_spline_eval(spline,t+dt),
        bezier_spline_eval(spline,t+2*dt)],"]"));*/
      polycyl_part(
  //    echo(
        i > 0 ? bezier_spline_eval(spline,t-dt) :
         closed ? bezier_spline_eval(spline,t1-dt) : false,
        bezier_spline_eval(spline,t),
        bezier_spline_eval(spline,t+dt),
        i < slices*N-1 ? bezier_spline_eval(spline,t+2*dt) :
         closed ? bezier_spline_eval(spline,t0+dt) : false,
        r);
    }
}

module splinecyl_test() {
  //points = [[0,0,1],[0,1,1],[2,1,1],[2,1,0],[3,1,0.5]];
  a = $t*360;
  d1 = [cos(a),sin(a),1];
  p = [2,1,0];
  points = [[0,0,1],[0,1,1],p+d1,p,p-d1,[0,0,-1],[0,0,0]];

  $fn = 20;
  scale(10) {
  for (p=points)
    translate(p)
      color([1,0,0])
        sphere(r=0.1);

    splinecyl(points,r=0.5,slices=60);
  //  polycyl([[0,0,0.2],[0,0,0],[cos($t*360),0,sin($t*360)]],r=0.5);
  //  polycyl([[0,0,1],[0,1,1],[2+cos($t*360),1+sin($t*360),1],[2,1,0],[3,1,0.5]],r=0.5);
  //  polycyl_part([0,0,1],[0,1,1],[2,1,1],[2,1,0],r=0.5);
  }
}

//splinecyl_test();

// === 2D spline stuff ===

// make a polygon between two splines.
// Each bezier line is evaluated at <slices> points.
// TODO: add external acceleration support.
module spline_area(spline=[],spline2=[],slices=max($fn,2),extern=false,closed=false) {
  N = floor((len(spline)-1)/3);
  dt = 1/slices;
  // we have a build system that allows to create objects like spline
  // cylinders externally, just dumping the name for the external utility
  // to get the arguments:
  t0 = 0;
  t1 = N;
  if (extern != false) {
    echo(str("extern module ",extern,": n",slices,":closed",closed?1:0,":",spline,":",spline2));
  }
  union()
    for (i=[0:slices*N-1],t=[i*dt]) {
      for (
        p1=[
          i > 0 ? bezier_spline_eval(spline,t-dt) :
           closed ? bezier_spline_eval(spline,t1-dt) : false],
        p2=[
          bezier_spline_eval(spline,t),
        ],
        p3=[
          bezier_spline_eval(spline,t+dt)
        ],
        p4=[
          i < slices*N-1 ? bezier_spline_eval(spline,t+2*dt) :
          closed ? bezier_spline_eval(spline,t0+dt) : false
        ],
        q1=[
          i > 0 ? bezier_spline_eval(spline2,t-dt) :
           closed ? bezier_spline_eval(spline2,t1-dt) : false],
        q2=[
          bezier_spline_eval(spline2,t),
        ],
        q3=[
          bezier_spline_eval(spline2,t+dt)
        ],
        q4=[
          i < slices*N-1 ? bezier_spline_eval(spline2,t+2*dt) :
          closed ? bezier_spline_eval(spline2,t0+dt) : false
        ],
        m1=[p1 != false ? (p1+q1)/2 : false],
        m4=[p4 != false ? (p4+q4)/2 : false],
        points=[
          m1 != false ? (m4 != false ? [m4,p3,p2,m1,q2,q3] : [p3,p2,m1,q2,q3])
                      : (m4 != false ? [m4,p3,p2,q2,q3] : [p3,p2,q2,q3])
        ]
      )
        polygon(points=points);//[m4,p3,p2,m1,q2,q3]);
    }
}

// puzzle piece for horizontal connection
// geom may be used for individualization of puzzle pieces.
// FIXME: while "add" already works, "extend" is not yet supported.
module puzzle_end(w=1,add=0,extend=0,h=false,geom=[0,0,0,0,0,0]) {
  //geom=[0.2,0.6,0.35,0.6]) {
  h = h == false ? w/2 : h;
  add = add/w;
  extend = extend/w;
  // want a spline:
  // [0,0]+t[1,0]->[(1+w1)/2,y1]+[0,1]->[(1+w2)/2,y2]+[0,1]->[1/2,h]+[1,0]
  // but can't use array-fors yet...

  stdgeom=[0.2,0.6,0.35,0.6,0,0];
  geom = [(1+geom[0])*stdgeom[0],(1+geom[1])*stdgeom[1],
          (1+geom[2])*stdgeom[2],(1+geom[3])*stdgeom[3],
          geom[4]+stdgeom[4],geom[5]+stdgeom[5]
          ];
  

  w1 = geom[0]; w2 = geom[1]; y1=geom[2]; y2 = geom[3];
  shear_x = geom[4]; shear_xy = geom[5];

  function S(p) = [p[0]+shear_x+shear_xy*p[1],p[1]];
  function M(p) = [1-p[0],p[1]];


  p = [[0,0],[(1-w1)/2,y1],[(1-w2)/2,y2],[1/2,1]];
  d = [[0.4,0],[0,y1/2],[0,max((y2-y1)/2,(1-y2)/2)],[w2/3,0]];
  spline=[p[0],p[0]+d[0],S(p[1]-d[1]),S(p[1]),S(p[1]+d[1]),S(p[2]-d[2]),S(p[2]),S(p[2]+d[2]),S(p[3]-d[3]),S(p[3])];
  spline2=[M(p[0]),M(p[0]+d[0]),S(M(p[1]-d[1])),S(M(p[1])),S(M(p[1]+d[1])),S(M(p[2]-d[2])),S(M(p[2])),S(M(p[2]+d[2])),S(M(p[3]-d[3])),S(p[3])];
  points=[
    [0,0],
    [0,-add/h],
    [1,-add/h],
    [1,0],
    [0.5,0.01]
  ];
  scale([w,h])
    union() {
      spline_area(spline,spline2,slices=20);
      polygon(points=points);
    }
}

// === laser-cutting ===

// toothed joint between laser parts.
// TODO: Will there be actual use of this function in a 3D-printing context (warranting the "extend" parameter)? Should "add" be centered, too?
module laser_joint(i=0,l=10,h=[3,3],minlength=5,center=false,add=0,invert=false,n=false,extend=0,even=false) {
  // joint(0,x) shall fit perpendicularly into a hole of joint(1,x).
  // for holes at edges add=0.001 is recommended.
  // for spikes at edges, a translate([-offs,0]) joint(0,...,add=offs)
  //   is recommended.

  // values for center: true,false,"x","y"

  // most basic version would be:
  //square([h[1-i],l]);

  // we want an odd number of sections for symmetry, every second section
  // gets pushed out.
  oddness = even ? 0 : 1;
  //count = n==false ? floor((l/minlength-1)/2) : n;
  count0 = n==false ? floor((l/minlength-oddness)/2) : n;
  count = (count0 >= 1 || !even) ? count0 : 1;
  //dl = l/(2*count+1);
  dl = l/(2*count+oddness);
  h = len(h) == undef ? [h,h] : h;
  w = h[1-i];
  origin = center==false?[0,0]:center==true?[-w/2,-l/2]:
           center=="x"?[-w/2,0]:center=="y"?[0,-l/2]:[0,0];
  
  translate(origin-[1,1]*extend)
    if (!invert) {
      if (count <= 0) {
        translate([0,l/4])
          square([w+add,l/2]+[2,2]*extend);
      } else {
        for (i=[0:count-1]) {
          translate([0,dl*(2*i+1)])
            square([w+add,dl]+[2,2]*extend);
        }
      }
    } else {
      if (count <= 0) {
        square([w+add,l/4]+[2,2]*extend);
        translate([0,l*3/4])
          square([w+add,l/4]+[2,2]*extend);
      } else {
        for (i=[0:count-1+oddness]) {
          translate([0,dl*(2*i)])
            square([w+add,dl]+[2,2]*extend);
        }
      }
    }
}

// make a laser pattern that enables the part to be bent (around the x axis)
module bending_pattern(w=1,h=1,pattern=[4,2,1],adaptive=true,laser_radius=amlaser_radius) {
  laser_d = laser_radius;

  hole_w0 = pattern[0];
  strut_w0 = pattern[1];
  // this is the real hole size + epsilon. After offset processing, only epsilon is left. (we will still laser this line twice, though)
  hole_h = laser_d*2+0.001; //0.1;
  hole_d0 = pattern[2];

  dx0 = hole_w0+strut_w0;
  dx = adaptive ? w/round(w/dx0) : dx0;
  hole_d = adaptive ? h/round(h/hole_d0) : hole_d0;
  hole_w = dx/dx0*hole_w0;
  strut_w = dx/dx0*strut_w0;
  nx = ceil(w/dx)-1;
  ny = ceil(h/hole_d);
//  dx = adaptive ? 

  for (j=[1:ny-1]) {
    for (i=[0:nx+j%2]) {
      translate([i*dx-(j%2==0 ? 0 : dx/2)+strut_w/2,j*hole_d])
        square([hole_w,hole_h]);
    }
  }
}

// === virtuals ===

// anchored at left end.
module ruler(length = 10, dl = false, color = true,real = false, stroke=false, depth=false, linelength = false) {
  dl = dl != false ? dl : 1;
  count = floor(length/dl);
  longcount = floor(length/(10*dl));
  stroke = stroke!=false ? stroke : dl*0.1;
  depth = depth!=false ? depth : dl*0.1;
  linelength = linelength!=false ? linelength : dl*2;

  if (color != false) {
    color(color == true ? [0,0,0,1] : color)
      ruler(length=length,dl=dl,color=false,real=real);
  } else {
    if (real || !no_virtuals) {
      for (i = [0:count]) {
        translate([i*dl,0,0])
          if (depth == false)
            square(size=[(i%10==0?2:1)*stroke,(i%10==0?5:i%5==0?3:2)*linelength/2],center=false);
          else
            cube(size=[(i%10==0?2:1)*stroke,(i%10==0?5:i%5==0?3:2)*linelength/2,depth],center=false);
      }
/*      for (i = [0:longcount]) {
        translate([i*dl*10,0,0])
          cube(size=[0.2,5,0.1],center=false);
      }*/
    }
  }
}

// anchored at center.
module box_ruler(size = [10,10,10],real=false, center=true) {
  origin = center ? -size/2 : 0;
  translate(origin) {
    scale([1,-1,1])
      ruler(length=size[0],color=[1,0,0],real=real);
    rotate([0,0,90])
      ruler(length=size[1],color=[0,1,0],real=real);
    rotate([0,-90,0]) scale([1,-1,1])
      ruler(length=size[2],color=[1,0,1],real=real);
  }
}

// default anchoring is bottom-center. (change with front or center)
module generic_placeholder(size=[20,40,10],center=false,front=false,real=false) {
  // % does more than color() transparency can do.
  if (real || !no_virtuals) {
    %translate([0,front?-size[1]/2:0,(center||front)?0:size[2]/2]) {
      color([0,0,1,0.5])
      //render(convexity = 5)
        difference() {
          cube(size=size*1.001,center=true);
          cube(size=size*0.999,center=true);
          scale(size)
            sphere(r=0.7);
        }
      rotate([0,-90,0]) {
        color([1,0,0,0.5])
          cylinder(r=size[0]*0.01,h=size[0]/2,center=false);
      }
      rotate([-90,0,0]) {
        color([0,1,0,0.5])
          cylinder(r=size[1]*0.01,h=size[1]/2,center=false);
      }
      color([0.5,0,0.5,0.5])
        cylinder(r=size[2]*0.01,h=size[2]/2,center=false);
      box_ruler(size);
    }
  }
}


// === some simple stuff ===

module maybe_extrude(height=false,center,convexity,twist,slices) {
  if (height != false && height != undef) {
    linear_extrude(height=height,center=center,convexity=convexity,twist=twist,slices=slices) children();
  } else {
    children();
  }
}

module ring_cylinder(r1=1,r2=false,h=1,center=false) {
  r2 = r2==false ? r1/2 : r2;
  difference() {
    cylinder(r=r1,h=h);
    translate([0,0,-0.05])
      cylinder(r=r2,h=h+0.1);
  }
}

// make a rather hollow wheel of radius R with n struts of thickness w and W (inside/outside) which can stand a bore hole of radius r. (convexity n/2+4)
module wheel_2d(R=2,r=1,W=0.1,w=0.1,n=8) {
  difference() {
    circle(r=R);
    difference() {
      circle(r=R-W);
      circle(r=r+w);
      for (i=[0:n-1]) {
        rotate(i/n*360)
          translate([0,-w/2])
            square([R,w]);
      }
    }
  }  
}

module wheel(R=2,r=1,W=0.1,w=0.1,n=8,h=false) {
  maybe_extrude(height=h)
    wheel_2d(R,r,W,w,n);
}

module torus(R=2,r=1,r_h=false,$fn=$fn,angle=false) {
  r_h = r_h != false ? r_h : r;
  N = 4;
  intersection() {
    rotate_extrude(convexity=4,$fn=2*$fn)
      translate([R,0])
        scale([1,r_h/r])
          circle(r);
    if (angle != false) {
      linear_extrude(height=r_h*2,center=true) 
        union() 
          for (i=[0:2*N-2]) {
            rotate(i*angle/(2*N))
              polygon([[0,0],2*[R+r+0.1,0],2*(R+r+0.1)*[cos(angle/N),sin(angle/N)]]);
          }
    }
  }
}

//linear_extrude(height=10,convexity=6)
//  wheel_2d(10,1,2,1,6);

// TODO: add support for width (instead of r1,r2)?
module angular_counter_strip(r1=0.5,r2=1,n=8,duty_cycle=false,length=false,height=false,h=1,angle=360) {
  height = height != false ? height : h;
  rL = max(r1,r2);
  rS = min(r1,r2);
  r1 = rS;
  r2 = rL;
  //da = 360/n;
  da = angle/n;
  length = duty_cycle != false ? duty_cycle*da :
               length != false ? length : da/2;
  // make a polygon of N edges to over-estimate circle in intersection.
  N = 6;
  phi = length/N;
  R = r2/cos(phi/2)+0.1;
  //R*[cos(phi*i),sin(phi*i)]

  maybe_extrude(height=height,convexity=2*N)
    intersection() {
      difference() {
        circle(r=r2);
        circle(r=r1);
      }
      union() {
        for (i = [0:n-1],
             v0=[[cos(i*da),sin(i*da)]],
             p1=[R*[cos(i*da+1*phi),sin(i*da+1*phi)]],
             p2=[R*[cos(i*da+2*phi),sin(i*da+2*phi)]],
             p3=[R*[cos(i*da+3*phi),sin(i*da+3*phi)]],
             p4=[R*[cos(i*da+4*phi),sin(i*da+4*phi)]],
             p5=[R*[cos(i*da+5*phi),sin(i*da+5*phi)]],
             vN=[[cos(i*da+length),sin(i*da+length)]]
            ) {
          polygon(points=[v0*r1*0.9,v0*R,
                           p1,p2,p3,p4,p5,
                          vN*R,vN*r1*0.9]);
        }
      }
    }
}

// works with size=[x,y] or height=false in 2D.
module linear_counter_strip(size=false,n=8,duty_cycle=false,width=1,length=1,height=1) {
  da = 360/n;
  size = size != false ? size : [width,length,height];
  dl = size[1]/n;
  ll = duty_cycle != false ? duty_cycle*dl : dl/2;

  maybe_extrude(height=size[2])
    for (i = [0:n-1]) {
      translate([0,i*dl,0])
        square(size=[size[0],ll]);
  }
}

// a cylinder with a hat (for printing), starting at z=-0.1.
// Use instead of normal cylinder.
// Note: This is actually useless, since normal cylinders are printable.
module bore_hole(r=1,h=1,slope=2,base=0.1) {
  hat_h = r*slope;
  total_h = h+hat_h+base+0.1;
  r_out = total_h/slope;
  translate([0,0,-base])
    intersection() {
      cylinder(r=r,h=total_h);
      translate([0,0,-0.1])
        cylinder(r1=r_out,r2=0,h=total_h);
    }
}

// print overhanging horizontal planes by removing parts to get a sparse set of
// resting points
// TODO: shift the lattice to make off-block cone tips impossible.
// FIXME: This is so useless. Don't use, bridges are a real printable thing.
module print_roughening(size=[1,1],slope=2,dx=1,base=0.1,extend=0.01,center=false) {
  have_height = len(size) >= 3;
  height = have_height ? size[2]-0.1 : slope*(dx/sqrt(2));
  dx = min(dx,height/slope*sqrt(2));
  h = slope*dx;
  sz = [size[0]+2*extend,size[1]+2*extend,h];
  offset = center ? -[size[0]/2,size[1]/2,have_height?size[2]/2:0] : [0,0,0];
  translate(offset)
    difference() {
      translate([-extend,-extend,-base])
        cube(size=sz+[0,0,base]);
      for (i = [0:ceil(sz[0]/dx)],j=[0:ceil(sz[1]/dx)])
        translate([i*dx,j*dx,0])
          cylinder(r1=0,r2=(h+0.1)/slope,h=h+0.1,$fn=8);
    }
}

// r1 > r2
// a screw based on a rotating decentralized circle
module simple_screw(length=1,rounds=10,r=1,r1=false,r2=false,cutout=false,base=1,extend=slide_backlash,tip=false,tip_slope=2) {
  r1 = r1 != false ? r1 : r;
  r2 = r2 != false ? r2 : r1*0.9;
  delta = cutout ? extend : 0;
  r_circle = (r1+r2)/2+delta;
  center = [(r1-r2)/2,0];
  da = 360*rounds*delta/length;

  slices = full_detail ? 12 : 4;

  union() {
    linear_extrude(height=length+delta,twist=360*rounds+da,slices=slices*rounds,convexity=2*rounds)
      translate(center)
        circle(r=r_circle);
    translate([0,0,-base])
      cylinder(r=cutout?r1+delta:r2,h=base+delta);
    if (tip) {
      translate([0,0,length+delta])
        bore_hole(r=r2+delta,h=0,slope=tip_slope);
    }
  }
}




