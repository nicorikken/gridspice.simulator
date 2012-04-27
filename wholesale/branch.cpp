/** $Id: branch.cpp 858 2008-08-28 21:06:13Z d3g637 $
	@file branch.cpp
	@defgroup branch Template for a new object class
	@ingroup MODULENAME

	You can add an object class to a module using the \e add_class
	command:
	<verbatim>
	linux% add_class CLASS
	</verbatim>

	You must be in the module directory to do this.

 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "branch.h"

CLASS *branch::oclass = NULL;
branch *branch::defaults = NULL;

#ifdef OPTIONAL
/* TODO: define this to allow the use of derived classes */
CLASS *PARENTbranch::pclass = NULL;
#endif

/* TODO: remove passes that aren't needed */
static PASSCONFIG passconfig = PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN;

/* TODO: specify which pass the clock advances */
static PASSCONFIG clockpass = PC_BOTTOMUP;

/* Class registration is only called once to register the class with the core */
branch::branch(MODULE *module)
#ifdef OPTIONAL
/* TODO: include this if you are deriving this from a superclass */
: SUPERCLASS(module)
#endif
{
#ifdef OPTIONAL
	/* TODO: include this if you are deriving this from a superclass */
	pclass = SUPERCLASS::oclass;
#endif
	if (oclass==NULL)
	{
		oclass = gl_register_class(module,"branch",sizeof(branch),passconfig);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s", __FILE__);

		if (gl_publish_variable(oclass,
              PT_int16, "F_BUS", PADDR(F_BUS),          // "from" bus number
              PT_int16, "T_BUS", PADDR(T_BUS),          // "to" bus number
	      PT_char1024, "from", PADDR(from), // "from" bus name
	      PT_char1024, "to", PADDR(to), // "to" bus name
              PT_double, "BR_R", PADDR(BR_R),           // resistance (p.u.)
              PT_double, "BR_X", PADDR(BR_X),           // reactance (p.u.)
              PT_double, "BR_B", PADDR(BR_B),           // total line charging susceptance (p.u.)
              PT_double, "RATE_A", PADDR(RATE_A),       // MVA rating A (long term rating)
              PT_double, "RATE_B", PADDR(RATE_B),       // MVA rating B (short term rating)
              PT_double, "RATE_C", PADDR(RATE_C),       // MVA rating C (emergency rating)
              PT_double, "TAP", PADDR(TAP),             // transformer off nominal turns ratio
                                                        // ( taps at "from" bus, impedance at "to"
                                                        // bus, i.e. if r=x=0, tap = \frac{|V_f|}{|V_t|}
              PT_double, "SHIFT", PADDR(SHIFT),         // transformer phase shift angle (degrees)
              PT_int16,	 "BR_STATUS", PADDR(BR_STATUS),   // initial branch status, 1 = in-service,0=out-of-service
              PT_double, "ANGMIN", PADDR(ANGMIN),       // minimum angle difference, \theta_f - \theta_t (degrees)
              PT_double, "ANGMAX", PADDR(ANGMAX),       // maximum angle difference, \theta_f - \theta_t (degrees)
		PT_double, "PF", PADDR(PF),
		PT_double, "QF", PADDR(QF),
		PT_double, "PT", PADDR(PT),
		PT_double, "QT", PADDR(QT),
		PT_double, "MU_SF", PADDR(MU_SF),
		PT_double, "MU_ST", PADDR(MU_ST),
		PT_double, "MU_ANGMIN", PADDR(MU_ANGMIN),
		PT_double, "MU_ANGMAX", PADDR(MU_ANGMAX),
           
              NULL)<1 && errno) GL_THROW("unable to publish properties in %s",__FILE__);
		defaults = this;
		memset(this,0,sizeof(branch));
		/* TODO: set the default values of all properties here */
	}
}

/* Object creation is called once for each object that is created by the core */
int branch::create(void)
{
	memcpy(this,defaults,sizeof(branch));
	/* TODO: set the context-free initial value of properties, such as random distributions */
	return 1; /* return 1 on success, 0 on failure */
}

/* Object initialization is called once after all object have been created */
int branch::init(OBJECT *parent)
{
	/* TODO: set the context-dependent initial value of properties */
	return 1; /* return 1 on success, 0 on failure */
}

/* Presync is called when the clock needs to advance on the first top-down pass */
TIMESTAMP branch::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	TIMESTAMP t2 = TS_NEVER;
	/* TODO: implement pre-topdown behavior */
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

/* Sync is called when the clock needs to advance on the bottom-up pass */
TIMESTAMP branch::sync(TIMESTAMP t0, TIMESTAMP t1)
{
	TIMESTAMP t2 = TS_NEVER;
	/* TODO: implement bottom-up behavior */
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

/* Postsync is called when the clock needs to advance on the second top-down pass */
TIMESTAMP branch::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	TIMESTAMP t2 = TS_NEVER;
	/* TODO: implement post-topdown behavior */
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int create_branch(OBJECT **obj)
{
	try
	{
		*obj = gl_create_object(branch::oclass);
		if (*obj!=NULL)
			return OBJECTDATA(*obj,branch)->create();
	}
	catch (char *msg)
	{
		gl_error("create_branch: %s", msg);
	}
	return 0;
}

EXPORT int init_branch(OBJECT *obj, OBJECT *parent)
{
	try
	{
		if (obj!=NULL)
			return OBJECTDATA(obj,branch)->init(parent);
	}
	catch (char *msg)
	{
		gl_error("init_branch(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
	}
	return 0;
}

EXPORT TIMESTAMP sync_branch(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	TIMESTAMP t2 = TS_NEVER;
	branch *my = OBJECTDATA(obj,branch);
	try
	{
		switch (pass) {
		case PC_PRETOPDOWN:
			t2 = my->presync(obj->clock,t1);
			break;
		case PC_BOTTOMUP:
			t2 = my->sync(obj->clock,t1);
			break;
		case PC_POSTTOPDOWN:
			t2 = my->postsync(obj->clock,t1);
			break;
		default:
			GL_THROW("invalid pass request (%d)", pass);
			break;
		}
		if (pass==clockpass)
			obj->clock = t1;
		return t2;
	}
	catch (char *msg)
	{
		gl_error("sync_branch(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
	}
	return TS_INVALID;
}
