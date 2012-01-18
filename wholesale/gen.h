/** $Id: gen.h 683 2008-06-18 20:16:29Z d3g637 $
	@file gen.h
	@addtogroup gen
	@ingroup MODULENAME

 @{
 **/

#ifndef _gen_H
#define _gen_H

#include <stdarg.h>
#include "gridlabd.h"

class gen {
private:
	/* TODO: put private variables here */
protected:
	/* TODO: put unpublished but inherited variables */
public:
	/* TODO: put published variables here */
	static CLASS *oclass;
	static gen *defaults;

	// variable
	int	GEN_BUS;
	double	PG;
	double 	QG;
	double 	QMAX;
	double 	QMIN;
	double 	VG;
	double	MBASE;
	double	GEN_STATUS;
	double	PMAX;
	double	PMIN;
	double 	PC1;
	double 	PC2;
	double 	QC1MIN;
	double 	QC1MAX;
	double	QC2MIN;
	double 	QC2MAX;
	double 	RAMP_AGC;
	double 	RAMP_10;
	double 	RAMP_30;
	double	RAMP_Q;
	double	APF;
public:
	/* required implementations */
	gen(MODULE *module);
	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP presync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP postsync(TIMESTAMP t0, TIMESTAMP t1);
public:
	
#ifdef OPTIONAL
	static CLASS *pclass; /**< defines the parent class */
	TIMESTAMP plc(TIMESTAMP t0, TIMESTAMP t1); /**< defines the default PLC code */
#endif
};

#endif

/**@}*/
