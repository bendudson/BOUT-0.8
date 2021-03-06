/**************************************************************************
 * Copyright 2010 B.D.Dudson, S.Farley, M.V.Umansky, X.Q.Xu
 *
 * Contact: Ben Dudson, bd512@york.ac.uk
 * 
 * This file is part of BOUT++.
 *
 * BOUT++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BOUT++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.
 *
 **************************************************************************/

#include "globals.h"

#include "mpi.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "field3d.h"
#include "utils.h"
#include "fft.h"
#include "dcomplex.h"
#include "interpolation.h"

/// Constructor
Field3D::Field3D()
{
#ifdef MEMDEBUG
  output.write("Field3D %u: constructor\n", (unsigned int) this);
#endif
#ifdef TRACK
  name = "<F3D>";
#endif

  /// Mark data as unallocated
  block = NULL;

  location = CELL_CENTRE; // Cell centred variable by default
}

/// Doesn't copy any data, just create a new reference to the same data (copy on change later)
Field3D::Field3D(const Field3D& f)
{
#ifdef MEMDEBUG
  output.write("Field3D %u: Copy constructor from %u\n", (unsigned int) this, (unsigned int) &f);
#endif

#ifdef CHECK
  msg_stack.push("Field3D: Copy constructor");
  f.check_data();
#endif

  /// Copy a reference to the block
  block = f.block;
  /// Increase reference count
  block->refs++;

  location = f.location;

#ifdef CHECK
  msg_stack.pop();
#endif
}

Field3D::~Field3D()
{
  /// free the block of data if allocated
  free_data();
}

Field3D* Field3D::clone() const
{
  return new Field3D(*this);
}

/// Make sure data is allocated and unique to this object
/// NOTE: Logically constant, but internals mutable
void Field3D::Allocate() const
{
  alloc_data();
}

real*** Field3D::getData() const
{
#ifdef CHECK
  if(block ==  NULL) {
    error("Field3D: getData() returning null pointer\n");
    exit(1);
  }

  // NOTE: Can't check data here since may not be set yet.
  //       Using this function circumvents the checking
#endif
  
  // User might alter data, so need to make unique
  Allocate();

  return(block->data);
}

const Field2D Field3D::DC()
{
  Field2D result;
  int jx, jy, jz;
  real **d;

#ifdef CHECK
  msg_stack.push("Field3D: DC");
  check_data();
#endif

#ifdef TRACK
  result.name = "DC(" + name + ")";
#endif

  result = 0.0;
  d = result.getData();

  real inv_n = 1. / (real) (ngz-1);

  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++) {
      for(jz=0;jz<(ngz-1);jz++)
	d[jx][jy] += block->data[jx][jy][jz];
      d[jx][jy] *= inv_n;
    }
  
#ifdef CHECK
  msg_stack.pop();
#endif

  return(result);
}

void Field3D::setLocation(CELL_LOC loc)
{
  if(loc == CELL_VSHIFT) {
    error("Field3D: CELL_VSHIFT cell location only makes sense for vectors");
  }
  
  if(loc == CELL_DEFAULT)
    loc = CELL_CENTRE;
  
  location = loc;
}

CELL_LOC Field3D::getLocation() const
{
  return location;
}

/***************************************************************
 *                         OPERATORS 
 ***************************************************************/

real** Field3D::operator[](int jx) const
{
#ifdef CHECK
  if(block == NULL) {
    error("Field3D: [] operator on empty data");
    exit(1);
  }
  
  if((jx < 0) || (jx >= ngx)) {
    error("Field3D: [%d] operator out of bounds", jx);
    exit(1);
  }
#endif
 
  // User might alter data, so need to make unique
  Allocate();
 
  return(block->data[jx]);
}

real& Field3D::operator[](bindex &bx) const
{
#ifdef CHECK
  if(block == NULL) {
    error("Field3D: [bindex] operator on empty data");
    exit(1);
  }
  if((bx.jx < 0) || (bx.jx >= ngx)) {
    error("Field3D: [bindex.jx = %d] out of range", bx.jx);
  }
  if((bx.jy < 0) || (bx.jy >= ngy)) {
    error("Field3D: [bindex.jy = %d] out of range", bx.jy);
  }
  if((bx.jz < 0) || (bx.jz >= ngz)) {
    error("Field3D: [bindex.jz = %d] out of range", bx.jz);
  }
#endif

  return block->data[bx.jx][bx.jy][bx.jz];
}

/////////////////// ASSIGNMENT ////////////////////

Field3D & Field3D::operator=(const Field3D &rhs)
{
  /// Check for self-assignment
  if(this == &rhs)
    return(*this); // skip this assignment

#ifdef CHECK

  msg_stack.push("Field3D: Assignment from Field3D");
  rhs.check_data(true);

#endif

#ifdef TRACK
  name = rhs.name;
#endif

  if(block != NULL)
    free_data(); // get rid of the current data

  /// Copy reference, don't copy data
  block = rhs.block;
  block->refs++;
  
  location = rhs.location;
  
#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator=(const Field2D &rhs)
{
  int jx, jy, jz;
  real **d;

#ifdef CHECK
  msg_stack.push("Field3D: Assignment from Field2D");
  rhs.check_data(true);
#endif

  d = rhs.getData();

#ifdef TRACK
  name = "F3D("+rhs.name+")";
#endif

  Allocate();

  /// Copy data

  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	block->data[jx][jy][jz] = d[jx][jy];

  /// Only 3D fields have locations
  //location = CELL_CENTRE;

#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator=(const FieldPerp &rhs)
{
  int jx, jy, jz; 
  real **d;
  
  jy = rhs.getIndex();
  
  d = rhs.getData();

#ifdef CHECK
  if(d == (real**) NULL) {
    // No data
    error("Field3D: No data in assignment from FieldPerp");
  }
  
  /// Test rhs values
  for(jx=MXG;jx<ngx-MXG;jx++)
    for(jz=0;jz<ngz;jz++)
      if(!finite(d[jx][jz])) {
	error("Field3D: Assignment from non-finite FieldPerp data at (%d,%d,%d)\n", jx,jy,jz);
      }
#endif

#ifdef TRACK
  name = "F3D("+rhs.name+")";
#endif

  Allocate();

  /// Copy data

  for(jx=0;jx<ngx;jx++)
    for(jz=0;jz<ngz;jz++)
      block->data[jx][jy][jz] = d[jx][jz];

  return(*this);
}

const bvalue & Field3D::operator=(const bvalue &bv)
{
  Allocate();

#ifdef CHECK
  if(!finite(bv.val)) {
    output.write("Field3D: assignment from non-finite value at (%d,%d,%d)\n", 
	   bv.jx, bv.jy,bv.jz);
    exit(1);
  }
#endif

#ifdef TRACK
  name = "<bv3D>";
#endif

  block->data[bv.jx][bv.jy][bv.jz] = bv.val;
  
  return(bv);
}

real Field3D::operator=(const real val)
{
  int jx, jy, jz;
  
  Allocate();

#ifdef CHECK
  if(!finite(val)) {
    error("Field3D: Assignment from non-finite real\n");
  }
#endif

#ifdef TRACK
  name = "<r3D>";
#endif

  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	block->data[jx][jy][jz] = val;

  // Only 3D fields have locations
  //location = CELL_CENTRE;
  // DON'T RE-SET LOCATION

  return(val);
}

////////////////// ADDITION //////////////////////

Field3D & Field3D::operator+=(const Field3D &rhs)
{
  int jx, jy, jz;

#ifdef CHECK
  msg_stack.push("Field3D: += Field3D");
  
  rhs.check_data();
  check_data();
#endif

  if(StaggerGrids && (rhs.location != location)) {
    // Interpolate and call again
    return (*this) += interp_to(rhs, location);
  }

#ifdef TRACK
  name = "(" + name + "+" + rhs.name + ")";
#endif

  if(block->refs == 1) {
    // This is the only reference to this data
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] += rhs.block->data[jx][jy][jz];
  }else {
    // Need to put result in a new block

    memblock3d *nb = new_block();

    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] + rhs.block->data[jx][jy][jz];

    block->refs--;
    block = nb;
  }

#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator+=(const Field2D &rhs)
{
  int jx, jy, jz;
  real **d;

#ifdef CHECK
  msg_stack.push("Field3D: += ( Field2D )");

  check_data();
  rhs.check_data();
#endif

  d = rhs.getData();

#ifdef TRACK
  name = "(" + name + "+" + rhs.name + ")";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] += d[jx][jy];
  }else {
    memblock3d *nb = new_block();
    
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] + d[jx][jy];

    block->refs--;
    block = nb;
  }

#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator+=(const real &rhs)
{
  int jx, jy, jz;
#ifdef CHECK
  msg_stack.push("Field3D: += ( real )");

  check_data();

  if(!finite(rhs)) {
    error("Field3D: += operator passed non-finite real number");
  }
#endif
  
#ifdef TRACK
  name = "(" + name + "+real)";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] += rhs;
  }else {
    memblock3d *nb = new_block();
    
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] + rhs;

    block->refs--;
    block = nb;
  }

#ifdef CHECK
  msg_stack.pop();
#endif
  
  return *this;
}

/////////////////// SUBTRACTION /////////////////////////

Field3D & Field3D::operator-=(const Field3D &rhs)
{
  int jx, jy, jz;

#ifdef CHECK
  msg_stack.push("Field3D: -= ( Field3D )");
  rhs.check_data();
  check_data();
#endif

  if(StaggerGrids && (rhs.location != location)) {
    // Interpolate and call again
    return (*this) -= interp_to(rhs, location);
  }

#ifdef TRACK
  name = "(" + name + "-" + rhs.name + ")";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] -= rhs.block->data[jx][jy][jz];
  }else {
    memblock3d *nb = new_block();
    
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] - rhs.block->data[jx][jy][jz];

    block->refs--;
    block = nb;
  }
  
#ifdef CHECK
  msg_stack.pop();
#endif
  
  return(*this);
}

Field3D & Field3D::operator-=(const Field2D &rhs)
{
  int jx, jy, jz;
  real **d;

#ifdef CHECK
  msg_stack.push("Field3D: -= ( Field2D )");
  rhs.check_data();
  check_data();
#endif

  d = rhs.getData();

#ifdef TRACK
  name = "(" + name + "-" + rhs.name + ")";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] -= d[jx][jy];

  }else {
    memblock3d *nb = new_block();

    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] - d[jx][jy];

    block->refs--;
    block = nb;
  }

#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator-=(const real &rhs)
{
  int jx, jy, jz;

#ifdef CHECK
  msg_stack.push("Field3D: -= ( real )");
  check_data();

  if(!finite(rhs)) {
    error("Field3D: -= operator passed non-finite real number");
  }
#endif

#ifdef TRACK
  name = "(" + name + "-real)";
#endif
  
  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] -= rhs;
  }else {
    memblock3d *nb = new_block();
    
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] - rhs;

    block->refs--;
    block = nb;
  }
  
#ifdef CHECK
  msg_stack.pop();
#endif

  return *this;
}

/////////////////// MULTIPLICATION ///////////////////////

Field3D & Field3D::operator*=(const Field3D &rhs)
{
  int jx, jy, jz;

#ifdef CHECK
  msg_stack.push("Field3D: *= ( Field3D )");

  rhs.check_data();
  check_data();
#endif

  if(StaggerGrids && (rhs.location != location)) {
    // Interpolate and call again
    return (*this) *= interp_to(rhs, location);
  }

#ifdef TRACK
  name = "(" + name + "*" + rhs.name + ")";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] *= rhs.block->data[jx][jy][jz];
  }else {
    memblock3d *nb = new_block();
    
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] * rhs.block->data[jx][jy][jz];

    block->refs--;
    block = nb;
  }

#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator*=(const Field2D &rhs)
{
  int jx, jy, jz;
  real **d;

#ifdef CHECK
  msg_stack.push("Field3D: *= ( Field2D )");
  rhs.check_data();
  check_data();
#endif

  d = rhs.getData();

#ifdef TRACK
  name = "(" + name + "*"+rhs.name+")";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] *= d[jx][jy];
  }else {
    memblock3d *nb = new_block();

    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] * d[jx][jy];

    block->refs--;
    block = nb;
  }

#ifdef CHECK
  msg_stack.pop();
#endif
  
  return(*this);
}

Field3D & Field3D::operator*=(const real rhs)
{
  int jx, jy, jz;
  
#ifdef CHECK
  msg_stack.push("Field3D: *= ( real )");
  check_data();

  if(!finite(rhs)) {
    error("Field3D: *= operator passed non-finite real number");
  }
#endif

#ifdef TRACK
  name = "(" + name + "*real)";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] *= rhs;

  }else {
    memblock3d *nb = new_block();

    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] * rhs;

    block->refs--;
    block = nb;
  }

#ifdef CHECK
  msg_stack.pop();
#endif
  
  return(*this);
}

//////////////////// DIVISION /////////////////////

Field3D & Field3D::operator/=(const Field3D &rhs)
{
  int jx, jy, jz;

  if(StaggerGrids && (rhs.location != location)) {
    // Interpolate and call again
    
#ifdef CHECK
    msg_stack.push("Field3D /= Interpolating");
#endif
    (*this) /= interp_to(rhs, location);
#ifdef CHECK
    msg_stack.pop();
#endif
    return *this;
  }

#ifdef CHECK
  msg_stack.push("Field3D: /= ( Field3D )");
  rhs.check_data();
  check_data();
#endif

#ifdef TRACK
  name = "(" + name + "/" + rhs.name+")";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] /= rhs.block->data[jx][jy][jz];
    
  }else {
    memblock3d *nb = new_block();

    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] / rhs.block->data[jx][jy][jz];

    block->refs--;
    block = nb;
  }

#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator/=(const Field2D &rhs)
{
  int jx, jy, jz;
  real **d;

#ifdef CHECK
  msg_stack.push("Field3D: /= ( Field2D )");
  rhs.check_data();
  check_data();
#endif

  d = rhs.getData();

#ifdef TRACK
  name = "(" + name + "/" + rhs.name+")";
#endif

  /// NOTE: The faster version of this uses multiplications rather than division
  /// This seems to be enough to make a difference to the number of CVODE steps
  /// Hence for now straight division is used

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++) {
	real val = 1.0L / d[jx][jy]; // Because multiplications are faster than divisions
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] *= val;
	  //block->data[jx][jy][jz] /= d[jx][jy];
      }
  }else {
    memblock3d *nb = new_block();

    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++) {
	real val = 1.0L / d[jx][jy];
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] * val;
	//nb->data[jx][jy][jz] = block->data[jx][jy][jz] / d[jx][jy];
      }

    block->refs--;
    block = nb;
  }
  
#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator/=(const real rhs)
{
  int jx, jy, jz;
  
#ifdef CHECK
  msg_stack.push("Field3D: /= ( real )");
  check_data();

  if(!finite(rhs)) {
    error("Field3D: /= operator passed non-finite real number");
  }
#endif

#ifdef TRACK
  name = "(" + name + "/real)";
#endif

  real val = 1.0 / rhs; // Because multiplication faster than division

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] *= val;
  }else {
    memblock3d *nb = new_block();
    
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = block->data[jx][jy][jz] * val;

    block->refs--;
    block = nb;
  }
  
#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

////////////////// EXPONENTIATION ///////////////////////

Field3D & Field3D::operator^=(const Field3D &rhs)
{
  int jx, jy, jz;

  if(StaggerGrids && (rhs.location != location)) {
    // Interpolate and call again
    
#ifdef CHECK
    msg_stack.push("Field3D ^= Interpolating");
#endif
    (*this) ^= interp_to(rhs, location);
#ifdef CHECK
    msg_stack.pop();
#endif
    return *this;
  }

#ifdef CHECK
  msg_stack.push("Field3D: ^= ( Field3D )");
  rhs.check_data();
  check_data();
#endif

#ifdef TRACK
  name = "(" + name + "^"+rhs.name+")";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] = pow(block->data[jx][jy][jz], rhs.block->data[jx][jy][jz]);

  }else {
    memblock3d *nb = new_block();
    
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = pow(block->data[jx][jy][jz], rhs.block->data[jx][jy][jz]);
    
    block->refs--;
    block = nb;
  }

#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator^=(const Field2D &rhs)
{
  int jx, jy, jz;
  real **d;

#ifdef CHECK
  msg_stack.push("Field3D: ^= ( Field2D )");
  rhs.check_data();
  check_data();
#endif

  d = rhs.getData();

#ifdef TRACK
  name = "(" + name + "^"+rhs.name+")";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] = pow(block->data[jx][jy][jz], d[jx][jy]);

  }else {
    memblock3d *nb = new_block();

    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = pow(block->data[jx][jy][jz], d[jx][jy]);

    block->refs--;
    block = nb;
  }
  
#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}

Field3D & Field3D::operator^=(const real rhs)
{
  int jx, jy, jz;

#ifdef CHECK
  msg_stack.push("Field3D: ^= ( real )");
  check_data();

  if(!finite(rhs)) {
    error("Field3D: ^= operator passed non-finite real number");
  }
#endif

#ifdef TRACK
  name = "(" + name + "^real)";
#endif

  if(block->refs == 1) {
    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  block->data[jx][jy][jz] = pow(block->data[jx][jy][jz], rhs);

  }else {
    memblock3d *nb = new_block();

    for(jx=0;jx<ngx;jx++)
      for(jy=0;jy<ngy;jy++)
	for(jz=0;jz<ngz;jz++)
	  nb->data[jx][jy][jz] = pow(block->data[jx][jy][jz], rhs);

    block->refs--;
    block = nb;
  }
  
#ifdef CHECK
  msg_stack.pop();
#endif

  return(*this);
}


/***************************************************************
 *                      BINARY OPERATORS 
 ***************************************************************/

/////////////////// ADDITION ///////////////////

const Field3D Field3D::operator+() const
{
  Field3D result = *this;

#ifdef TRACK
  result.name = name;
#endif
  
  return result;
}


const Field3D Field3D::operator+(const Field3D &other) const
{
  Field3D result = *this;
  result += other;
  return(result);
}

const Field3D Field3D::operator+(const Field2D &other) const
{
  Field3D result = *this;
  result += other;
  return(result);
}

const FieldPerp Field3D::operator+(const FieldPerp &other) const
{
  FieldPerp result = other;
  result += (*this);
  return(result);
}

const Field3D Field3D::operator+(const real &rhs) const
{
  Field3D result = *this;
  result += rhs;
  return(result);
}

/////////////////// SUBTRACTION ////////////////

const Field3D Field3D::operator-() const
{
  Field3D result = *this;

  result *= -1.0;

#ifdef TRACK
  result.name = "(-"+name+")";
#endif
  
  return result;
}

const Field3D Field3D::operator-(const Field3D &other) const
{
  Field3D result = *this;
  result -= other;
  return(result);
}

const Field3D Field3D::operator-(const Field2D &other) const
{
  Field3D result = *this;
  result -= other;
  return(result);
}

const FieldPerp Field3D::operator-(const FieldPerp &other) const
{
  real **d;
  int jx, jz;
  int jy = other.getIndex();
  FieldPerp result = other;

#ifdef CHECK
  if(block == NULL)
    error("Field3D: - FieldPerp operates on empty data");
#endif

  d = result.getData();
  for(jx=0;jx<ngx;jx++)
    for(jz=0;jz<ngz;jz++)
      d[jx][jz] = block->data[jx][jy][jz] - d[jx][jz];
  
  return(result);
}

const Field3D Field3D::operator-(const real &rhs) const
{
  Field3D result = *this;
  result -= rhs;
  return(result);
}

///////////////// MULTIPLICATION ///////////////

const Field3D Field3D::operator*(const Field3D &other) const
{
  Field3D result = *this;
  result *= other;
  return(result);
}

const Field3D Field3D::operator*(const Field2D &other) const
{
  Field3D result = *this;
  result *= other;
  return(result);
}

const FieldPerp Field3D::operator*(const FieldPerp &other) const
{
  FieldPerp result = other;
  result *= (*this);
  return(result);
}

const Field3D Field3D::operator*(const real rhs) const
{
  Field3D result = *this;
  result *= rhs;
  return(result);
}

//////////////////// DIVISION ////////////////////

const Field3D Field3D::operator/(const Field3D &other) const
{
  Field3D result = *this;
  result /= other;
  return(result);
}

const Field3D Field3D::operator/(const Field2D &other) const
{
  Field3D result = *this;
  result /= other;
  return(result);
}

const FieldPerp Field3D::operator/(const FieldPerp &other) const
{
  real **d;
  int jx, jz;
  int jy = other.getIndex();
  FieldPerp result = other;
  
#ifdef CHECK
  if(block == NULL)
    error("Field3D: / FieldPerp operates on empty data");
#endif

  d = result.getData();
  for(jx=0;jx<ngx;jx++)
    for(jz=0;jz<ngz;jz++)
      d[jx][jz] = block->data[jx][jy][jz] / d[jx][jz];
  
#ifdef TRACK
  result.name = "(" + name + "/" + other.name + ")";
#endif  
  
  return(result);
}

const Field3D Field3D::operator/(const real rhs) const
{
  Field3D result = *this;
  result /= rhs;
  return(result);
}

////////////// EXPONENTIATION /////////////////

const Field3D Field3D::operator^(const Field3D &other) const
{
  Field3D result = *this;
  result ^= other;
  return(result);
}

const Field3D Field3D::operator^(const Field2D &other) const
{
  Field3D result = *this;
  result ^= other;
  return(result);
}

const FieldPerp Field3D::operator^(const FieldPerp &other) const
{
  real **d;
  int jx, jz;
  int jy = other.getIndex();
  FieldPerp result = other;
  
#ifdef CHECK
  if(block == NULL)
    error("Field3D: ^ FieldPerp operates on empty data");
#endif

  d = result.getData();
  for(jx=0;jx<ngx;jx++)
    for(jz=0;jz<ngz;jz++)
      d[jx][jz] = pow(block->data[jx][jy][jz], d[jx][jz]);
  
#ifdef TRACK
  result.name = "("+name+"^"+other.name + ")";
#endif

  return(result);
}

const Field3D Field3D::operator^(const real rhs) const
{
  Field3D result = *this;
  result ^= rhs;
  return(result);
}

/***************************************************************
 *                         STENCILS
 ***************************************************************/

/*!
  18 Aug 2008: Added need_x argument to disable Z interpolation when not needed
  since interp_z is taking ~25% of run time (on single processor)
*/
void Field3D::SetStencil(bstencil *fval, bindex *bx, bool need_x) const
{
  fval->jx = bx->jx;
  fval->jy = bx->jy;
  fval->jz = bx->jz;
  
#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: Setting stencil for empty data\n");
    exit(1);
  }
#endif

  fval->cc = block->data[bx->jx][bx->jy][bx->jz];

  if(need_x) {
    if(ShiftXderivs) {
      fval->xp = interp_z(bx->jxp, bx->jy, bx->jz, bx->xp_offset, ShiftOrder);
      fval->xm = interp_z(bx->jxm, bx->jy, bx->jz, bx->xm_offset, ShiftOrder);
      fval->x2p = interp_z(bx->jx2p, bx->jy, bx->jz, bx->x2p_offset, ShiftOrder);
      fval->x2m = interp_z(bx->jx2m, bx->jy, bx->jz, bx->x2m_offset, ShiftOrder);
    }else {
      // No shift in the z direction
      fval->xp = block->data[bx->jxp][bx->jy][bx->jz];
      fval->xm = block->data[bx->jxm][bx->jy][bx->jz];
      fval->x2p = block->data[bx->jx2p][bx->jy][bx->jz];
      fval->x2m = block->data[bx->jx2m][bx->jy][bx->jz];
    }
  }

  // TWIST-SHIFT CONDITION
  if(bx->yp_shift) {
    fval->yp = interp_z(bx->jx, bx->jyp, bx->jz, bx->yp_offset, TwistOrder);
  }else
    fval->yp = block->data[bx->jx][bx->jyp][bx->jz];
  
  if(bx->ym_shift) {
    fval->ym = interp_z(bx->jx, bx->jym, bx->jz, bx->ym_offset, TwistOrder);
  }else
    fval->ym = block->data[bx->jx][bx->jym][bx->jz];

  if(bx->y2p_shift) {
    fval->y2p = interp_z(bx->jx, bx->jy2p, bx->jz, bx->yp_offset, TwistOrder);
  }else
    fval->y2p = block->data[bx->jx][bx->jy2p][bx->jz];

  if(bx->y2m_shift) {
    fval->y2m = interp_z(bx->jx, bx->jy2m, bx->jz, bx->ym_offset, TwistOrder);
  }else
    fval->y2m = block->data[bx->jx][bx->jy2m][bx->jz];

  // Z neighbours
  
  fval->zp = block->data[bx->jx][bx->jy][bx->jzp];
  fval->zm = block->data[bx->jx][bx->jy][bx->jzm];
  fval->z2p = block->data[bx->jx][bx->jy][bx->jz2p];
  fval->z2m = block->data[bx->jx][bx->jy][bx->jz2m];
}

void Field3D::SetXStencil(stencil &fval, const bindex &bx, CELL_LOC loc) const
{
  fval.jx = bx.jx;
  fval.jy = bx.jy;
  fval.jz = bx.jz;
  
#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: Setting X stencil for empty data\n");
    exit(1);
  }
#endif

  fval.c = block->data[bx.jx][bx.jy][bx.jz];

  if(ShiftXderivs && (ShiftOrder != 0)) {
    fval.p = interp_z(bx.jxp, bx.jy, bx.jz, bx.xp_offset, ShiftOrder);
    fval.m = interp_z(bx.jxm, bx.jy, bx.jz, bx.xm_offset, ShiftOrder);
    fval.pp = interp_z(bx.jxp, bx.jy, bx.jz, bx.x2p_offset, ShiftOrder);
    fval.mm = interp_z(bx.jxm, bx.jy, bx.jz, bx.x2m_offset, ShiftOrder);
  }else {
    // No shift in the z direction
    fval.p = block->data[bx.jxp][bx.jy][bx.jz];
    fval.m = block->data[bx.jxm][bx.jy][bx.jz];
    fval.pp = block->data[bx.jx2p][bx.jy][bx.jz];
    fval.mm = block->data[bx.jx2m][bx.jy][bx.jz];
  }

  if(StaggerGrids && (loc != CELL_DEFAULT) && (loc != location)) {
    // Non-centred stencil

    if((location == CELL_CENTRE) && (loc == CELL_XLOW)) {
      // Producing a stencil centred around a lower X value
      fval.pp = fval.p;
      fval.p  = fval.c;
      
    }else if(location == CELL_XLOW) {
      // Stencil centred around a cell centre
      
      fval.mm = fval.m;
      fval.m  = fval.c;
    }
    // Shifted in one direction -> shift in another
    // Could produce warning
  }
}

void Field3D::SetYStencil(stencil &fval, const bindex &bx, CELL_LOC loc) const
{
  fval.jx = bx.jx;
  fval.jy = bx.jy;
  fval.jz = bx.jz;
  
#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: Setting Y stencil for empty data\n");
    exit(1);
  }
#endif

  fval.c = block->data[bx.jx][bx.jy][bx.jz];
  
  if((!TwistShift) || (TwistOrder == 0)) {
    // Either no twist-shift, or already done in communicator
    
    fval.p = block->data[bx.jx][bx.jyp][bx.jz];
    fval.m = block->data[bx.jx][bx.jym][bx.jz];
    fval.pp = block->data[bx.jx][bx.jy2p][bx.jz];
    fval.mm = block->data[bx.jx][bx.jy2m][bx.jz];
    
  }else {
    // TWIST-SHIFT CONDITION
    if(bx.yp_shift) {
      fval.p = interp_z(bx.jx, bx.jyp, bx.jz, bx.yp_offset, TwistOrder);
    }else
      fval.p = block->data[bx.jx][bx.jyp][bx.jz];
    
    if(bx.ym_shift) {
      fval.m = interp_z(bx.jx, bx.jym, bx.jz, bx.ym_offset, TwistOrder);
    }else
      fval.m = block->data[bx.jx][bx.jym][bx.jz];
    
    if(bx.y2p_shift) {
      fval.pp = interp_z(bx.jx, bx.jy2p, bx.jz, bx.yp_offset, TwistOrder);
    }else
      fval.pp = block->data[bx.jx][bx.jy2p][bx.jz];
    
    if(bx.y2m_shift) {
      fval.mm = interp_z(bx.jx, bx.jy2m, bx.jz, bx.ym_offset, TwistOrder);
    }else
      fval.mm = block->data[bx.jx][bx.jy2m][bx.jz];
  }

  if(StaggerGrids && (loc != CELL_DEFAULT) && (loc != location)) {
    // Non-centred stencil

    if((location == CELL_CENTRE) && (loc == CELL_YLOW)) {
      // Producing a stencil centred around a lower Y value
      fval.pp = fval.p;
      fval.p  = fval.c;
    }else if(location == CELL_YLOW) {
      // Stencil centred around a cell centre
      
      fval.mm = fval.m;
      fval.m  = fval.c;
    }
    // Shifted in one direction -> shift in another
    // Could produce warning
  }
}

void Field3D::SetZStencil(stencil &fval, const bindex &bx, CELL_LOC loc) const
{
  fval.jx = bx.jx;
  fval.jy = bx.jy;
  fval.jz = bx.jz;
  
#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: Setting stencil for empty data\n");
    exit(1);
  }
#endif

  fval.c = block->data[bx.jx][bx.jy][bx.jz];

  fval.p = block->data[bx.jx][bx.jy][bx.jzp];
  fval.m = block->data[bx.jx][bx.jy][bx.jzm];
  fval.pp = block->data[bx.jx][bx.jy][bx.jz2p];
  fval.mm = block->data[bx.jx][bx.jy][bx.jz2m];

  if(StaggerGrids && (loc != CELL_DEFAULT) && (loc != location)) {
    // Non-centred stencil

    if((location == CELL_CENTRE) && (loc == CELL_ZLOW)) {
      // Producing a stencil centred around a lower Z value
      fval.pp = fval.p;
      fval.p  = fval.c;
      
    }else if(location == CELL_ZLOW) {
      // Stencil centred around a cell centre
      
      fval.mm = fval.m;
      fval.m  = fval.c;
    }
    // Shifted in one direction -> shift in another
    // Could produce warning
  }
}

real Field3D::interp_z(int jx, int jy, int jz0, real zoffset, int order) const
{
  int zi;
  real result;
  int jzp, jzm, jz2p;

  zi = ROUND(zoffset);  // Find the nearest integer
  zoffset -= (real) zi; // Difference (-0.5 to +0.5)

  if((zoffset < 0.0) && (order > 1)) { // If order = 0 or 1, want closest
    // For higher-order interpolation, expect zoffset > 0
    zi--;
    zoffset += 1.0;
  }
  
  jz0 = (((jz0 + zi)%ncz) + ncz) % ncz;
  jzp = (jz0 + 1) % ncz;
  jz2p = (jz0 + 2) % ncz;
  jzm = (jz0 - 1 + ncz) % ncz;

  switch(order) {
  case 2: {
    // 2-point linear interpolation

    result = (1.0 - zoffset)*block->data[jx][jy][jz0] + zoffset*block->data[jx][jy][jzp];

    break;
  }
  case 3: {
    // 3-point Lagrange interpolation

    result = 0.5*zoffset*(zoffset-1.0)*block->data[jx][jy][jzm]
      + (1.0 - zoffset*zoffset)*block->data[jx][jy][jz0]
      + 0.5*zoffset*(zoffset + 1.0)*block->data[jx][jy][jzp];
    break;
  }
  case 4: {
    // 4-point Lagrange interpolation
    result = -zoffset*(zoffset-1.0)*(zoffset-2.0)*block->data[jx][jy][jzm]/6.0
      + 0.5*(zoffset*zoffset - 1.0)*(zoffset-2.0)*block->data[jx][jy][jz0]
      - 0.5*zoffset*(zoffset+1.0)*(zoffset-2.0)*block->data[jx][jy][jzp]
      + zoffset*(zoffset*zoffset - 1.0)*block->data[jx][jy][jz2p]/6.0;
    break;
  }
  default: {
    // Nearest neighbour
    result = block->data[jx][jy][jz0];
  }
  };
  return result;
}

void Field3D::ShiftZ(int jx, int jy, double zangle)
{
  static dcomplex *v = (dcomplex*) NULL;
  int jz;
  real kwave;
  
#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: Shifting in Z an empty data set\n");
    exit(1);
  }
#endif

  if(ncz == 1)
    return;

  Allocate();

  if(v == (dcomplex*) NULL) {
    // Allocate memory
    v = new dcomplex[ncz/2 + 1];
  }
  
  rfft(block->data[jx][jy], ncz, v); // Forward FFT

  // Apply phase shift
  for(jz=1;jz<=ncz/2;jz++) {
    kwave=jz*2.0*PI/zlength; // wave number is 1/[rad]
    v[jz] *= dcomplex(cos(kwave*zangle) , -sin(kwave*zangle));
  }

  irfft(v, ncz, block->data[jx][jy]); // Reverse FFT

  block->data[jx][jy][ncz] = block->data[jx][jy][0];
}

const Field3D Field3D::ShiftZ(const Field2D zangle) const
{
  Field3D result;
  int jx, jy;

#ifdef CHECK
  msg_stack.push("Field3D: ShiftZ ( Field2D )");
  check_data();
#endif

  result = *this;

  for(jx=0;jx<ngx;jx++) {
    for(jy=0;jy<ngy;jy++) {
      result.ShiftZ(jx, jy, zangle[jx][jy]);
    }
  }

#ifdef CHECK
  msg_stack.pop();
#endif

  return result;
}

const Field3D Field3D::ShiftZ(const real zangle) const
{
  Field3D result;
  int jx, jy;

#ifdef CHECK
  msg_stack.push("Field3D: ShiftZ ( real )");
  check_data();
#endif

  result = *this;

  for(jx=0;jx<ngx;jx++) {
    for(jy=0;jy<ngy;jy++) {
      result.ShiftZ(jx, jy, zangle);
    }
  }

#ifdef CHECK
  msg_stack.pop();
#endif

  return result;
}

const Field3D Field3D::ShiftZ(bool toreal) const
{
  if(toreal) {
    return ShiftZ(zShift);
  }
  return ShiftZ(-zShift);
}


/***************************************************************
 *                         SLICING
 ***************************************************************/

void Field3D::getXarray(int y, int z, rvec &xv) const
{
#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: getXarray on an empty data set\n");
    exit(1);
  }
#endif

  xv.resize(ngx);
  
  for(int x=0;x<ngx;x++)
    xv[x] = block->data[x][y][z];
}

void Field3D::getYarray(int x, int z, rvec &yv) const
{
#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: getYarray on an empty data set\n");
    exit(1);
  }
#endif

  yv.resize(ngy);
  
  for(int y=0;y<ngy;y++)
    yv[y] = block->data[x][y][z];
}

void Field3D::getZarray(int x, int y, rvec &zv) const
{
#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: getZarray on an empty data set\n");
    exit(1);
  }
#endif

  zv.resize(ngz-1);
  
  for(int z=0;z<ngz-1;z++)
    zv[z] = block->data[x][y][z];
}

void Field3D::setXarray(int y, int z, const rvec &xv)
{
  Allocate();

#ifdef CHECK
  // Check that vector is correct size
  if(xv.capacity() != (unsigned int) ngx) {
    error("Field3D: setXarray has incorrect size\n");
    exit(1);
  }
#endif

  for(int x=0;x<ngx;x++)
    block->data[x][y][z] = xv[x];
}

void Field3D::setYarray(int x, int z, const rvec &yv)
{
  Allocate();

#ifdef CHECK
  // Check that vector is correct size
  if(yv.capacity() != (unsigned int) ngy) {
    error("Field3D: setYarray has incorrect size\n");
    exit(1);
  }
#endif

  for(int y=0;y<ngy;y++)
    block->data[x][y][z] = yv[y];
}

void Field3D::setZarray(int x, int y, const rvec &zv)
{
  Allocate();

#ifdef CHECK
  // Check that vector is correct size
  if(zv.capacity() != (unsigned int) (ngz-1)) {
    error("Field3D: setZarray has incorrect size\n");
    exit(1);
  }
#endif

  for(int z=0;z<(ngz-1);z++)
    block->data[x][y][z] = zv[z];
}

const FieldPerp Field3D::Slice(int y) const
{
  FieldPerp result;

  result.Set(*this, y);

#ifdef TRACK
  result.name = "Slice("+name+")";
#endif

  return(result);
}

/***************************************************************
 *                      MATH FUNCTIONS
 ***************************************************************/

const Field3D Field3D::Sqrt() const
{
  int jx, jy, jz;
  Field3D result;

#ifdef CHECK
  msg_stack.push("Field3D: Sqrt()");

  // Check data set
  if(block == NULL) {
    error("Field3D: Taking sqrt of empty data\n");
    exit(1);
  }
    
  // Test values
  for(jx=MXG;jx<ngx-MXG;jx++)
    for(jy=MYG;jy<ngy-MYG;jy++) 
      for(jz=0;jz<ncz;jz++) {
	if(block->data[jx][jy][jz] < 0.0) {
	  error("Field3D: Sqrt operates on negative value at [%d,%d,%d]\n", jx, jy, jz);
	}
      }
#endif

#ifdef TRACK
  result.name = "Sqrt("+name+")";
#endif

  result.Allocate();

  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++) 
      for(jz=0;jz<ngz;jz++)
	result.block->data[jx][jy][jz] = sqrt(block->data[jx][jy][jz]);

#ifdef CHECK
  msg_stack.pop();
#endif

  result.location = location;
  
  return result;
}

const Field3D Field3D::Abs() const
{
  int jx, jy, jz;
  Field3D result;

#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: Taking abs of empty data\n");
    exit(1);
  }
#endif

#ifdef TRACK
  result.name = "Abs("+name+")";
#endif

  result.Allocate();

  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++) 
      for(jz=0;jz<ngz;jz++)
	result.block->data[jx][jy][jz] = fabs(block->data[jx][jy][jz]);

  result.location = location;

  return result;
}

real Field3D::Min(bool allpe) const
{
  int jx, jy, jz;
  real result;

#ifdef CHECK
  if(block == NULL) {
    error("Field3D: min() method on empty data");
    exit(1);
  }

  if(allpe) {
    msg_stack.push("Field3D::Min() over all PEs");
  }else
    msg_stack.push("Field3D::Min()");
#endif

  result = block->data[0][0][0];

  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	if(block->data[jx][jy][jz] < result)
	  result = block->data[jx][jy][jz];

  if(allpe) {
    // MPI reduce
    real localresult = result;
    MPI_Allreduce(&localresult, &result, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  }

#ifdef CHECK
  msg_stack.pop();
#endif

  return result;
}

real Field3D::Max(bool allpe) const
{
  int jx, jy, jz;
  real result;

#ifdef CHECK
  if(block == NULL) {
    error("Field3D: max() method on empty data");
    exit(1);
  }
  if(allpe) {
    msg_stack.push("Field3D::Max() over all PEs");
  }else
    msg_stack.push("Field3D::Max()");
#endif
  
  result = block->data[0][0][0];

  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	if(block->data[jx][jy][jz] > result)
	  result = block->data[jx][jy][jz];
  
  if(allpe) {
    // MPI reduce
    real localresult = result;
    MPI_Allreduce(&localresult, &result, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  }
  
#ifdef CHECK
  msg_stack.pop();
#endif

  return result;
}

///////////////////// FieldData VIRTUAL FUNCTIONS //////////

int Field3D::getData(int x, int y, int z, void *vptr) const
{
#ifdef CHECK
  // Check data set
  if(block ==  NULL) {
    error("Field3D: getData on empty data\n");
    exit(1);
  }
  
  // check ranges
  if((x < 0) || (x > ncx) || (y < 0) || (y > ncy) || (z < 0) || (z >= ncz)) {
    error("Field3D: getData (%d,%d,%d) out of bounds\n", x, y, z);
    exit(1);
  }
#endif
  real *ptr = (real*) vptr;
  *ptr = block->data[x][y][z];
  
  return sizeof(real);
}

int Field3D::getData(int x, int y, int z, real *rptr) const
{
#ifdef CHECK
  // Check data set
  if(block == NULL) {
    error("Field3D: getData on empty data\n");
    exit(1);
  }
  
  // check ranges
  if((x < 0) || (x > ncx) || (y < 0) || (y > ncy) || (z < 0) || (z >= ncz)) {
    error("Field3D: getData (%d,%d,%d) out of bounds\n", x, y, z);
    exit(1);
  }
#endif

  *rptr = block->data[x][y][z];
  return 1;
}

int Field3D::setData(int x, int y, int z, void *vptr)
{
  Allocate();
#ifdef CHECK
  // check ranges
  if((x < 0) || (x > ncx) || (y < 0) || (y > ncy) || (z < 0) || (z >= ncz)) {
    error("Field3D: fillArray (%d,%d,%d) out of bounds\n", x, y, z);
    exit(1);
  }
#endif
  real *ptr = (real*) vptr;
  block->data[x][y][z] = *ptr;
  
  return sizeof(real);
}

int Field3D::setData(int x, int y, int z, real *rptr)
{
  Allocate();
#ifdef CHECK
  // check ranges
  if((x < 0) || (x > ncx) || (y < 0) || (y > ncy) || (z < 0) || (z >= ncz)) {
    error("Field3D: setData (%d,%d,%d) out of bounds\n", x, y, z);
    exit(1);
  }
#endif

  block->data[x][y][z] = *rptr;
  return 1;
}

#ifdef CHECK
/// Check if the data is valid
bool Field3D::check_data(bool vital) const
{
  if(block ==  NULL) {
    error("Field3D: Operation on empty data\n");
    return true;
  }

  if( vital || ( CHECK > 2 ) ) { 
    // Do full checks
    // Normally this is done just for some operations (equalities)
    // If CHECKS > 2, all operations perform checks
    
    int jx, jy, jz;
    
    for(jx=MXG;jx<ngx-MXG;jx++)
      for(jy=MYG;jy<ngy-MYG;jy++)
	for(jz=0;jz<ncz;jz++)
	  if(!finite(block->data[jx][jy][jz])) {
	    error("Field3D: Operation on non-finite data at [%d][%d][%d]\n", jx, jy, jz);
	    return true;
	  }
  }
    
  return false;
}
#endif

/***************************************************************
 *                     PRIVATE FUNCTIONS
 ***************************************************************/

// GLOBAL VARS

int Field3D::nblocks = 0;
memblock3d* Field3D::blocklist = NULL;
memblock3d* Field3D::free_block = NULL;

/// Get a new block of data, either from free list or allocate
memblock3d *Field3D::new_block() const
{
  memblock3d *nb;

  if(free_block != NULL) {
    // just pop off the top of the stack
    nb = free_block;
    free_block = nb->next;
    nb->next = NULL;
    nb->refs = 1;
  }else {
    // No more blocks left - allocate a new block
    nb = new memblock3d;

    nb->data = r3tensor(ngx, ngy, ngz);
    nb->refs = 1;

    // add to the global list
    nb->next = blocklist;
    blocklist = nb;

    nblocks++;

    //output.write("Allocated: %d\n", nblocks);
  }

  return nb;
}


/// Makes sure data is allocated and only referenced by this object
void Field3D::alloc_data() const
{
  /// Check if any data associated with this object
  if(block != (memblock3d*) NULL) {
    // Already has a block of data
    
    /// Check if data shared with other objects
    if(block->refs > 1) {
      // Need to get a new block and copy across

      memblock3d* nb = new_block();

      for(int jx=0;jx<ngx;jx++)
	for(int jy=0;jy<ngy;jy++)
	  for(int jz=0;jz<ngz;jz++)
	    nb->data[jx][jy][jz] = block->data[jx][jy][jz];

      block->refs--;
      block = nb;
    }
  }else {
    // No data - get a new block

    block = new_block();
    
  }
}

void Field3D::free_data()
{
  // put data block onto stack

  if(block == NULL)
    return;

  block->refs--;

  if(block->refs == 0) {
    // No more references to this data - put on free list

    block->next = free_block;
    free_block = block;
  }

  block = NULL;
}

/***************************************************************
 *               NON-MEMBER OVERLOADED OPERATORS
 ***************************************************************/

const Field3D operator-(const real &lhs, const Field3D &rhs)
{
  Field3D result;
  int jx, jy, jz;

#ifdef TRACK
  result.name = "(real-"+rhs.name+")";
#endif

  result.Allocate();
  
  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	result.block->data[jx][jy][jz] = lhs - rhs.block->data[jx][jy][jz];

  result.location = rhs.location;

  return result;
}

const Field3D operator+(const real &lhs, const Field3D &rhs)
{
  return rhs+lhs;
}

const Field3D operator*(const real lhs, const Field3D &rhs)
{
  return(rhs * lhs);
}

const Field3D operator/(const real lhs, const Field3D &rhs)
{
  Field3D result = rhs;
  int jx, jy, jz;
  real ***d;

  d = result.getData();
#ifdef CHECK
  if(d == (real***) NULL) {
    bout_error("Field3D: left / operator has invalid Field3D argument");
  }
#endif

#ifdef TRACK
  result.name = "(real/"+rhs.name+")";
#endif
  
  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	d[jx][jy][jz] = lhs / d[jx][jy][jz];

  result.setLocation( rhs.getLocation() );

  return(result);
}

const Field3D operator^(const real lhs, const Field3D &rhs)
{
  Field3D result = rhs;
  int jx, jy, jz;
  real ***d;

  d = result.getData();

#ifdef CHECK
  if(d == (real***) NULL) {
    output.write("Field3D: left ^ operator has invalid Field3D argument");
    exit(1);
  }
#endif

#ifdef TRACK
  result.name = "(real^"+rhs.name+")";
#endif
  
  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	d[jx][jy][jz] = pow(lhs, d[jx][jy][jz]);

  result.setLocation( rhs.getLocation() );

  return(result);
}

//////////////// NON-MEMBER FUNCTIONS //////////////////

const Field3D sqrt(const Field3D &f)
{
  return f.Sqrt();
}

const Field3D abs(const Field3D &f)
{
  return f.Abs();
}

real min(const Field3D &f, bool allpe)
{
  return f.Min(allpe);
}

real max(const Field3D &f, bool allpe)
{
  return f.Max(allpe);
}

// Friend functions

const Field3D sin(const Field3D &f)
{
  Field3D result;
  int jx, jy, jz;
  
  result.Allocate();
  
  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	result.block->data[jx][jy][jz] = sin(f.block->data[jx][jy][jz]);

#ifdef TRACK
  result.name = "sin("+f.name+")";
#endif

  result.location = f.location;

  return result;
}

const Field3D cos(const Field3D &f)
{
  Field3D result;
  int jx, jy, jz;
  
  result.Allocate();
  
  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	result.block->data[jx][jy][jz] = cos(f.block->data[jx][jy][jz]);

#ifdef TRACK
  result.name = "cos("+f.name+")";
#endif

  result.location = f.location;

  return result;
}

const Field3D tan(const Field3D &f)
{
  Field3D result;
  int jx, jy, jz;
  
  result.Allocate();
  
  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	result.block->data[jx][jy][jz] = tan(f.block->data[jx][jy][jz]);

#ifdef TRACK
  result.name = "tan("+f.name+")";
#endif

  result.location = f.location;

  return result;
}

const Field3D sinh(const Field3D &f)
{
  Field3D result;
  int jx, jy, jz;
  
  result.Allocate();
  
  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	result.block->data[jx][jy][jz] = sinh(f.block->data[jx][jy][jz]);

#ifdef TRACK
  result.name = "sinh("+f.name+")";
#endif

  result.location = f.location;

  return result;
}

const Field3D cosh(const Field3D &f)
{
  Field3D result;
  int jx, jy, jz;
  
  result.Allocate();
  
  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	result.block->data[jx][jy][jz] = cosh(f.block->data[jx][jy][jz]);

#ifdef TRACK
  result.name = "cosh("+f.name+")";
#endif

  result.location = f.location;

  return result;
}

const Field3D tanh(const Field3D &f)
{
  Field3D result;
  int jx, jy, jz;
  
  result.Allocate();
  
  for(jx=0;jx<ngx;jx++)
    for(jy=0;jy<ngy;jy++)
      for(jz=0;jz<ngz;jz++)
	result.block->data[jx][jy][jz] = tanh(f.block->data[jx][jy][jz]);

#ifdef TRACK
  result.name = "tanh("+f.name+")";
#endif

  result.location = f.location;

  return result;
}

const Field3D filter(const Field3D &var, int N0)
{
  Field3D result;
  static dcomplex *f = (dcomplex*) NULL;
  int jx, jy, jz;
  
  if(f == (dcomplex*) NULL) {
    // Allocate memory
    f = new dcomplex[ncz/2 + 1];
  }

  result.Allocate();

  for(jx=0;jx<ngx;jx++) {
    for(jy=0;jy<ngy;jy++) {

      rfft(var.block->data[jx][jy], ncz, f); // Forward FFT

      for(jz=0;jz<=ncz/2;jz++) {
	
	if(jz != N0) {
	  // Zero this component
	  f[jz] = 0.0;
	}
      }

      irfft(f, ncz, result.block->data[jx][jy]); // Reverse FFT

      result.block->data[jx][jy][ncz] = result.block->data[jx][jy][0];
    }
  }
  
#ifdef TRACK
  result.name = "filter("+var.name+")";
#endif
  
  result.location = var.location;

  return result;
}

// Smooths a field in Fourier space
// DOESN'T WORK VERY WELL
/*
const Field3D smooth(const Field3D &var, real zmax, real xmax)
{
  Field3D result;
  static dcomplex **f = NULL, *fx;
  int jx, jy, jz, zmi, xmi;

  if(f == NULL) {
    f = cmatrix(ngx, ncz/2 + 1); 
    fx = new dcomplex[2*ngx];
  }
  
  if((zmax > 1.0) || (xmax > 1.0)) {
    // Removed everyting
    result = 0.0;
    return result;
  }
  
  result.Allocate();

  zmi = ncz/2;
  xmi = ngx;

  if(zmax > 0.0)
    zmi = (int) ((1.0 - zmax)*((real) (ncz/2)));

  if(xmax > 0.0)
    xmi = (int) ((1.0 - xmax)*((real) ngx));

  //output.write("filter: %d, %d\n", xmi, zmi);

  for(jy=0;jy<ngy;jy++) {

    for(jx=0;jx<ngx;jx++) {
      // Take FFT in the Z direction, shifting into real space
      ZFFT(var.block->data[jx][jy], zShift[jx][jy], f[jx]);
    }

    if(zmax > 0.0) {
      // filter in z
      for(jx=0;jx<ngx;jx++) {
	for(jz=zmi+1;jz<=ncz/2;jz++) {
	  f[jx][jz] = 0.0;
	}
      }
    }

    if(is_pow2(ngx) && (xmax > 0.0)) {
      // ngx is a power of 2 - filter in x too
      for(jz=0;jz<=zmi;jz++) { // Go through non-zero toroidal modes
	for(jx=0;jx<ngx;jx++) {
	  fx[jx] = f[jx][jz];
	  fx[2*ngx - 1 - jx] = f[jx][jz]; // NOTE:SYMMETRIC
	}
	
	// FFT in X direction
	
	cfft(fx, 2*ngx, -1); // FFT
	
	for(jx=xmi+1; jx<=ngx; jx++) {
	  fx[jx] = 0.0;
	  fx[2*ngx-jx] = 0.0;
	}
	
	// Reverse X FFT
	cfft(fx, 2*ngx, 1);

	for(jx=0;jx<ngx;jx++)
	  f[jx][jz] = fx[jx];
	
      }
    }

    // Reverse Z FFT
    for(jx=0;jx<ngx;jx++) {
      ZFFT_rev(f[jx], zShift[jx][jy], result.block->data[jx][jy]);
      result.block->data[jx][jy][ncz] = result.block->data[jx][jy][0];
    }
  }
  
  return result;
}
*/

// Fourier filter in z
const Field3D low_pass(const Field3D &var, int zmax)
{
  Field3D result;
  static dcomplex *f = NULL;
  int jx, jy, jz;

#ifdef CHECK
  msg_stack.push("low_pass(Field3D, %d)", zmax);
#endif

  if(!var.isAllocated())
    return var;

  if(f == NULL)
    f = new dcomplex[ncz/2 + 1];
 
  if((zmax >= ncz/2) || (zmax < 0)) {
    // Removing nothing
    return var;
  }
  
  result.Allocate();

  for(jx=0;jx<ngx;jx++) {
    for(jy=0;jy<ngy;jy++) {
      // Take FFT in the Z direction
      rfft(var.block->data[jx][jy], ncz, f);
      
      // Filter in z
      for(jz=zmax+1;jz<=ncz/2;jz++)
	f[jz] = 0.0;

      irfft(f, ncz, result.block->data[jx][jy]); // Reverse FFT
      result.block->data[jx][jy][ncz] = result.block->data[jx][jy][0];
    }
  }
  
  result.location = var.location;

#ifdef CHECK
  msg_stack.pop();
#endif
  
  return result;
}
// Fourier filter in z with zmin
const Field3D low_pass(const Field3D &var, int zmax, int zmin)
{
  Field3D result;
  static dcomplex *f = NULL;
  int jx, jy, jz;

#ifdef CHECK
  msg_stack.push("low_pass(Field3D, %d, %d)", zmax, zmin);
#endif

  if(!var.isAllocated())
    return var;

  if(f == NULL)
    f = new dcomplex[ncz/2 + 1];
 
  if((zmax >= ncz/2) || (zmax < 0)) {
    // Removing nothing
    return result;
  }
  
  result.Allocate();

  for(jx=0;jx<ngx;jx++) {
    for(jy=0;jy<ngy;jy++) {
      // Take FFT in the Z direction
      rfft(var.block->data[jx][jy], ncz, f);
      
      // Filter in z
      for(jz=zmax+1;jz<=ncz/2;jz++)
	f[jz] = 0.0;

      // Filter zonal mode
      if(zmin==0) {
	f[0] = 0.0;
      }
      irfft(f, ncz, result.block->data[jx][jy]); // Reverse FFT
      result.block->data[jx][jy][ncz] = result.block->data[jx][jy][0];
    }
  }
  
  result.location = var.location;

#ifdef CHECK
  msg_stack.pop();
#endif
  
  return result;
}

bool finite(const Field3D &f)
{
#ifdef CHECK
  msg_stack.push("finite( Field3D )");
#endif

  if(!f.isAllocated()) {
#ifdef CHECK
    msg_stack.pop();
#endif
    return false;
  }
  
  for(int jx=0;jx<ngx;jx++)
    for(int jy=0;jy<ngy;jy++)
      for(int jz=0;jz<ncz;jz++)
	if(!finite(f.block->data[jx][jy][jz])) {
#ifdef CHECK
	  msg_stack.pop();
#endif
	  return false;
	}

#ifdef CHECK
  msg_stack.pop();
#endif

  return true;
}
