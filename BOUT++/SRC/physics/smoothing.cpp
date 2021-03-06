/**************************************************************
 * Smoothing operators
 *
 *
 * 2010-05-17 Ben Dudson <bd512@york.ac.uk>
 *    * Added nonlinear filter
 * 
 **************************************************************************
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
 **************************************************************/

#include <math.h>

#include "communicator.h"

#include "smoothing.h"
#include "globals.h"

#include "bout_types.h"
// Smooth using simple 1-2-1 filter
const Field3D smooth_x(const Field3D &f, bool realspace)
{
  Field3D fs, result;

  if(realspace) {
    fs = f.ShiftZ(true); // Shift into real space
  }else
    fs = f; 

  result.Allocate();
  
  // Copy boundary region
  for(int jy=0;jy<ngy;jy++)
    for(int jz=0;jz<ngz;jz++) {
      result[0][jy][jz] = fs[0][jy][jz];
      result[ngx-1][jy][jz] = fs[ngx-1][jy][jz];
    }

  // Smooth using simple 1-2-1 filter

  for(int jx=1;jx<ngx-1;jx++)
    for(int jy=0;jy<ngy;jy++)
      for(int jz=0;jz<ngz;jz++) {
	result[jx][jy][jz] = 0.5*fs[jx][jy][jz] + 0.25*( fs[jx-1][jy][jz] + fs[jx+1][jy][jz] );
      }

  if(realspace)
    result = result.ShiftZ(false); // Shift back

  // Need to communicate boundaries
  Communicator c;
  c.add(result);
  c.run();

  return result;
}


const Field3D smooth_y(const Field3D &f)
{
  Field3D result;

  result.Allocate();
  
  // Copy boundary region
  for(int jx=0;jx<ngx;jx++)
    for(int jz=0;jz<ngz;jz++) {
      result[jx][0][jz] = f[jx][0][jz];
      result[jx][ngy-1][jz] = f[jx][ngy-1][jz];
    }
  
  // Smooth using simple 1-2-1 filter

  for(int jx=0;jx<ngx;jx++)
    for(int jy=1;jy<ngy-1;jy++)
      for(int jz=0;jz<ngz;jz++) {
	result[jx][jy][jz] = 0.5*f[jx][jy][jz] + 0.25*( f[jx][jy-1][jz] + f[jx][jy+1][jz] );
      }

  // Need to communicate boundaries
  Communicator c;
  c.add(result);
  c.run();

  return result;
}

// Define MPI operation to sum 2D fields over y.
// NB: Don't sum in y boundary regions
void ysum_op(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype)
{
    real *rin = (real*) invec;
    real *rinout = (real*) inoutvec;
    for(int x=0;x<ngx;x++) {
	real val = 0.;
	// Sum values
	for(int y=MYG;y<MYG+MYSUB;y++) {
	    val += rin[x*ngy + y] + rinout[x*ngy + y];
	}
	// Put into output (spread over y)
	val /= MYSUB;
	for(int y=0;y<ngy;y++)
	    rinout[x*ngy + y] = val;
    }
}

const Field2D average_y(const Field2D &f)
{
  static MPI_Op op;
  static bool opdefined = false;

#ifdef CHECK
  msg_stack.push("average_y(Field2D)");
#endif

  if(!opdefined) {
    MPI_Op_create(ysum_op, 1, &op);
    opdefined = true;
  }

  Field2D result;
  result.Allocate();
  
  real **fd, **rd;
  fd = f.getData();
  rd = result.getData();
  
  MPI_Allreduce(*fd, *rd, ngx*ngy, MPI_DOUBLE, op, comm_y);
  
  result /= (real) NYPE;

#ifdef CHECK
  msg_stack.pop();
#endif
  
  return result;
}

/// Nonlinear filtering to remove grid-scale noise
/*!
  From a paper:

  W.Shyy et. al. JCP 102 (1) September 1992 page 49

  "On the Suppression of Numerical Oscillations Using a Non-Linear Filter"
  
 */
void nl_filter(rvec &f, real w)
{
  for(int i=1; i<f.size()-1; i++) {
    
    real dp = f[i+1] - f[i];
    real dm = f[i-1] - f[i];
    if(dp*dm > 0.) {
      // Local extrema - adjust
      real ep, em, e; // Amount to adjust by
      if(fabs(dp) > fabs(dm)) {
	ep = w*0.5*dp;
	em = w*dm;
	e = (fabs(ep) < fabs(em)) ? ep : em; // Pick smallest absolute
	// Adjust
	f[i+1] -= e;
	f[i] += e;
      }else {
	ep = w*0.5*dm;
	em = w*dp;
	e = (fabs(ep) < fabs(em)) ? ep : em; // Pick smallest absolute
	// Adjust
	f[i-1] -= e;
	f[i] += e;
      }
    }
  }
}

const Field3D nl_filter_x(const Field3D &f, real w)
{
#ifdef CHECK
  msg_stack.push("nl_filter_x( Field3D )");
#endif
  
  Field3D fs;
  fs = f.ShiftZ(true); // Shift into real space
  Field3D result;
  rvec v;
  
  for(int jy=0;jy<ngy;jy++)
    for(int jz=0;jz<ncz;jz++) {
      fs.getXarray(jy, jz, v);
      nl_filter(v, w);
      result.setXarray(jy, jz, v);
    }
  
  result = result.ShiftZ(false); // Shift back

#ifdef CHECK
  msg_stack.pop();
#endif
  return result;
}

const Field3D nl_filter_y(const Field3D &fs, real w)
{
#ifdef CHECK
  msg_stack.push("nl_filter_x( Field3D )");
#endif
  
  Field3D result;
  rvec v;
  
  for(int jx=0;jx<ngx;jx++)
    for(int jz=0;jz<ncz;jz++) {
      fs.getYarray(jx, jz, v);
      nl_filter(v, w);
      result.setYarray(jx, jz, v);
    }
  
#ifdef CHECK
  msg_stack.pop();
#endif
  return result;
}

const Field3D nl_filter_z(const Field3D &fs, real w)
{
#ifdef CHECK
  msg_stack.push("nl_filter_x( Field3D )");
#endif
  
  Field3D result;
  rvec v;
  
  for(int jx=0;jx<ngx;jx++)
    for(int jy=0;jy<ngy;jy++) {
      fs.getZarray(jx, jy, v);
      nl_filter(v, w);
      result.setZarray(jx, jy, v);
    }

#ifdef CHECK
  msg_stack.pop();
#endif
  return result;
}

const Field3D nl_filter(const Field3D &f, real w)
{
  Field3D result;
  /// Perform filtering in Z, Y then X
  result = nl_filter_x(nl_filter_y(nl_filter_z(f, w), w), w);
  /// Communicate boundaries
  Communicator c;
  c.add(result);
  c.run();
  return result;
}
