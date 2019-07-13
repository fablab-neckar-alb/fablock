include <utilities.scad>;

/*
  Copyright (c) 2019 by Thomas Kremer
  License: GNU GPL ver. 2 or 3
*/

/* PARTS LIST:
  box:  box();
  rest: rest();
  lid:  lid();
*/
demo = !currently_building;
hires = !demo;
$fn = hires ? 120 : 60;

pinpad_w = 61;
pinpad_h = 81;
pinpad_d = 0.4;
pinpad_bus_w = 41;
pinpad_bus_l_top = 10;
pinpad_bus_l = 51;
pinpad_border_w = 4;
pinpad_buttons_d = 2;

cable_r = 2;
cable_size = [6,3];

pinpad_x = 6;
pinpad_y = pinpad_x;
wall_d = 1.5;
bottom_d = wall_d;
lid_d = 1;
rest_d = 2;
box_d = 2*pinpad_bus_l_top+bottom_d;

box_size=[pinpad_w+2*pinpad_x,pinpad_h+2*pinpad_x+pinpad_bus_l_top,box_d];
  
lid_z = box_size[2]-lid_d;
pinpad_z = lid_z-pinpad_d;
rest_z = pinpad_z-rest_d;

// screw = [r, h, head_r, head_h]
screw = [2.5/2,18+1,5/2,1.5];
// nut = [w,h]
nut = [5,2+1]; // want extra space for nut because it's a bridging print.

box_screw_r = 4/2;

// align our screw boxes with the outside of the case.
// The screws are basically right outside the pinpad.
screw_box_w = (pinpad_x-screw[0])*2;

screws = [for (y=[-1,1],x=[-1,1])
            [box_size[0]/2,box_size[1]/2,0]+
            [x*box_size[0]/2,y*box_size[1]/2,atan2(0,x)]
            -screw_box_w/2*[x,y,0]];
//            -(pinpad_x-screw[0])*[x,y,0]];

box_screws = [for (x=[-1,1]) [(x+2)*box_size[0]/4,box_size[1]*2/3]];

// TODO: get the screw hole module into utilities.scad
module screw_hole(screw_r=3/2, screw_h=10, head_r=3, nut_w=5, nut_h=2, nut_z=2, depth=false, head_z=false, with_nut = false, screw=false, nut=false, infty = 100, extend=0, diaphragm_up=false, diaphragm_down=false, diaphragm_h = 0.3) {
  screw_r = screw != false ? screw[0] : screw_r;
  screw_h = screw != false ? screw[1] : screw_h;
  head_r = screw != false ? screw[2] : head_r;
  //head_h = screw != false ? screw[3] : head_h;
  nut_w = nut != false ? nut[0] : nut_w;
  nut_h = nut != false ? nut[1] : nut_h;
  with_nut = nut != false ? true : with_nut;

  //nut_z = depth != false ? nut_z+depth : nut_z;
  z = depth != false ? -depth : head_z != false ? head_z-screw_h : 0;
  nut_r = nut_w/sqrt(3);
  offs = extend;
  difference() {
    translate([0,0,z-offs])
      cylinder(r=screw_r+offs,h=screw_h+1);
    if (diaphragm_up) {
      translate([0,0,nut_z+nut_h+offs])
        cylinder(r=screw_r+offs+1,h=diaphragm_h);
    }
    if (diaphragm_down) {
      translate([0,0,nut_z-offs-diaphragm_h])
        cylinder(r=screw_r+offs+1,h=diaphragm_h);
      translate([0,0,z+screw_h-offs-diaphragm_h])
        cylinder(r=screw_r+offs+1,h=diaphragm_h);
    }
  }
  translate([0,0,z+screw_h-offs])
    cylinder(r=head_r+offs,h=infty-screw_h+2*offs);
  if (with_nut)
    translate([-nut_w/2,-nut_r,nut_z]-[1,1,1]*offs)
      cube([nut_w,nut_r+infty,nut_h]+[2,2,2]*offs);
}


module pinpad_hw() {
  eps = 0.01;
  color([0.7,0.7,0.7])
  linear_extrude(height=pinpad_d) {
    square([pinpad_w,pinpad_h]);
    translate([pinpad_w/2-pinpad_bus_w/2,pinpad_bus_l_top])
      square([pinpad_bus_w,pinpad_h]);
  }
  color([0.5,0.5,0.5])
  translate([pinpad_border_w,pinpad_border_w,eps])
    linear_extrude(height=pinpad_buttons_d-eps) {
      square([pinpad_w,pinpad_h]-[2,2]*pinpad_border_w);
  }
}

module box() {
  //screw_box_w = nut[0]+3*wall_d;//2*screw[0]+wall_d;
  //screw_box_w = screw_box_w+0.1;
  difference() {
    cube(box_size);
    difference() {
      translate([1,1,0]*(wall_d-slide_backlash)+[0,0,bottom_d])
        cube(box_size-[2,2,0]*(wall_d-slide_backlash));
      for (p2=screws,p=[[p2[0],p2[1],0]],ang=[p2[2]]) {
        difference() {
          translate(p-[1,1]*screw_box_w/2)
            cube([1,1,0]*screw_box_w+[0,0,rest_z]);
          translate(p)
            rotate([0,0,ang+90])
              screw_hole(screw=screw,nut=nut,head_z=box_size[2],nut_z=10,infty=50,diaphragm_up=true,extend=slide_backlash);
        }
      }
    }
    for (p=box_screws) {
      translate([p[0],p[1],-1])
        cylinder(r=box_screw_r,h=2+bottom_d);
    }
    translate([-1,box_size[1]-screw_box_w-cable_size[0]-1,bottom_d+1])
      cube([2+box_size[0],cable_size[0],cable_size[1]]);
    /*translate([-1,box_size[1]-screw_box_w-cable_r-1,bottom_d+cable_r+1])
      rotate([0,90,0])
        cylinder(r=cable_r,h=2+box_size[0]);*/
  }
}

module rest() {
  clearance = slide_backlash;
  union() {
    for (i=[0,1])
      linear_extrude(height=i?rest_d+pinpad_d:rest_d) difference() {
        square([box_size[0]-2*wall_d,box_size[1]-2*wall_d]);
        if (i)
          translate([1,1]*(pinpad_x-wall_d-clearance))
            square([pinpad_w,pinpad_h]+[2,2]*clearance);
        translate([pinpad_w/2-pinpad_bus_w/2,pinpad_h]+[1,1]*(pinpad_x-wall_d-clearance))
          square([pinpad_bus_w,pinpad_bus_l_top]+[2,2]*clearance);
        for (p2=screws,p=[[p2[0],p2[1]]]) {
          translate(p-[1,1]*wall_d)
            circle(screw[0]);
        }
      }
  }
}

module lid() {
  linear_extrude(height=lid_d) difference() {
    square([box_size[0]-2*wall_d,box_size[1]-2*wall_d]);
    translate([1,1]*(pinpad_x-wall_d+pinpad_border_w-1))
      square([pinpad_w,pinpad_h]-[2,2]*(pinpad_border_w-1));
    for (p2=screws,p=[[p2[0],p2[1]]]) {
      translate(p-[1,1]*wall_d)
        circle(screw[0]);
    }
  }
}

module case() {
  box();
  translate([wall_d,wall_d,rest_z])
    rest();
  translate([pinpad_x,pinpad_y,pinpad_z])
    pinpad_hw();
  translate([wall_d,wall_d,lid_z])
    lid();
}

if (demo) {
  //pinpad_hw();
  translate([-box_size[0]/2,-box_size[1]/2,0])
    case();
}


