/** $Id: battery.cpp,v 1.2 2008/02/12 00:28:08 d3g637 Exp $
	Copyright (C) 2008 Battelle Memorial Institute
	@file battery.cpp
	@defgroup battery
	@ingroup generators

 @{
 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "generators.h"

#include "energy_storage.h"
#include "battery.h"
#include "gridlabd.h"




#define HOUR 3600 * TS_SECOND

CLASS *battery::oclass = NULL;
battery *battery::defaults = NULL;

static PASSCONFIG passconfig = PC_BOTTOMUP|PC_POSTTOPDOWN;
static PASSCONFIG clockpass = PC_BOTTOMUP;

/* Class registration is only called once to register the class with the core */
battery::battery(MODULE *module)
{
	if (oclass==NULL)
	{
		oclass = gl_register_class(module,"battery",sizeof(battery),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN);
		if (oclass==NULL)
			throw "unable to register class battery";
		else
			oclass->trl = TRL_PROOF;

		if (gl_publish_variable(oclass,
			PT_enumeration,"generator_mode",PADDR(gen_mode_v),
				PT_KEYWORD,"UNKNOWN",GM_UNKNOWN,
				PT_KEYWORD,"CONSTANT_V",GM_CONSTANT_V,
				PT_KEYWORD,"CONSTANT_PQ",GM_CONSTANT_PQ,
				PT_KEYWORD,"CONSTANT_PF",GM_CONSTANT_PF,
				PT_KEYWORD,"SUPPLY_DRIVEN",GM_SUPPLY_DRIVEN, //PV must operate in this mode
				PT_KEYWORD,"POWER_DRIVEN",GM_POWER_DRIVEN,
				PT_KEYWORD,"VOLTAGE_CONTROLLED",GM_VOLTAGE_CONTROLLED,
				PT_KEYWORD,"POWER_VOLTAGE_HYBRID",GM_POWER_VOLTAGE_HYBRID,

			PT_enumeration,"additional_controls",PADDR(additional_controls),
				PT_KEYWORD,"NONE",AC_NONE,
				PT_KEYWORD,"LINEAR_TEMPERATURE",AC_LINEAR_TEMPERATURE,

			PT_enumeration,"generator_status",PADDR(gen_status_v),
				PT_KEYWORD,"OFFLINE",OFFLINE,
				PT_KEYWORD,"ONLINE",ONLINE,	

			PT_enumeration,"rfb_size", PADDR(rfb_size_v),
				PT_KEYWORD, "HOUSEHOLD", HOUSEHOLD,
				PT_KEYWORD, "SMALL", SMALL, 
				PT_KEYWORD, "MED_COMMERCIAL", MED_COMMERCIAL,
				PT_KEYWORD, "MED_HIGH_ENERGY", MED_HIGH_ENERGY,
				PT_KEYWORD, "LARGE", LARGE,

			PT_enumeration,"power_type",PADDR(power_type_v),
				PT_KEYWORD,"AC",AC,
				PT_KEYWORD,"DC",DC,

			PT_enumeration,"battery_state",PADDR(battery_state),
				PT_KEYWORD,"CHARGING",BS_CHARGING,
				PT_KEYWORD,"DISCHARGING",BS_DISCHARGING,
				PT_KEYWORD,"WAITING",BS_WAITING,
				PT_KEYWORD,"FULL",BS_FULL,
				PT_KEYWORD,"EMPTY",BS_EMPTY,
				PT_KEYWORD,"CONFLICTED",BS_CONFLICTED,

			PT_double, "number_battery_state_changes", PADDR(no_of_cycles),

			// Used for linear temperature control mode
			PT_double, "monitored_power[W]", PADDR(check_power),
			PT_double, "power_set_high[W]", PADDR(power_set_high),
			PT_double, "power_set_low[W]", PADDR(power_set_low),
			PT_double, "power_set_high_highT[W]", PADDR(power_set_high_highT),
			PT_double, "power_set_low_highT[W]", PADDR(power_set_low_highT),
			PT_double, "check_power_low[W]", PADDR(check_power_low),
			PT_double, "check_power_high[W]", PADDR(check_power_high),
			PT_double, "voltage_set_high[V]", PADDR(voltage_set_high),
			PT_double, "voltage_set_low[V]", PADDR(voltage_set_low),
			PT_double, "deadband[V]", PADDR(deadband),
			PT_double, "sensitivity", PADDR(sensitivity),
			PT_double, "high_temperature", PADDR(high_temperature),
			PT_double, "midpoint_temperature", PADDR(midpoint_temperature),
			PT_double, "low_temperature", PADDR(low_temperature),

			//Used for PQ mode
			PT_double, "scheduled_power[W]", PADDR(B_scheduled_power),

			PT_double, "Rinternal[Ohm]", PADDR(Rinternal),
			PT_double, "V_Max[V]", PADDR(V_Max), 
			PT_complex, "I_Max[A]", PADDR(I_Max),
			PT_double, "E_Max[Wh]", PADDR(E_Max),
			PT_double, "P_Max[W]", PADDR(Max_P),
			PT_double, "power_factor", PADDR(pf),
			PT_double, "Energy[Wh]",PADDR(Energy),
			PT_double, "efficiency[unit]", PADDR(efficiency),
			PT_double, "base_efficiency[unit]", PADDR(base_efficiency),
			PT_double, "parasitic_power_draw[W]", PADDR(parasitic_power_draw),


			//PT_int64, "generator_mode_choice", PADDR(generator_mode_choice),

			PT_double, "Rated_kVA[kVA]", PADDR(Rated_kVA),
			//PT_double, "Rated_kV[kV]", PADDR(Rated_kV),
			PT_complex, "V_Out[V]", PADDR(V_Out),
			PT_complex, "I_Out[A]", PADDR(I_Out),
			PT_complex, "VA_Out[VA]", PADDR(VA_Out),
			PT_complex, "V_In[V]", PADDR(V_In),
			PT_complex, "I_In[A]", PADDR(I_In),
			PT_complex, "V_Internal[V]", PADDR(V_Internal),
			PT_complex, "I_Internal[A]",PADDR(I_Internal),
			PT_complex, "I_Prev[A]", PADDR(I_Prev),

			PT_double,"power_transferred",PADDR(power_transferred),

			//resistasnces and max P, Q

			PT_set, "phases", PADDR(phases),

				PT_KEYWORD, "A",(set)PHASE_A,
				PT_KEYWORD, "B",(set)PHASE_B,
				PT_KEYWORD, "C",(set)PHASE_C,
				PT_KEYWORD, "N",(set)PHASE_N,
				PT_KEYWORD, "S",(set)PHASE_S,
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
		defaults = this;
		memset(this,0,sizeof(battery));
		/* TODO: set the default values of all properties here */
	}
}

/* Object creation is called once for each object that is created by the core */
int battery::create(void) 
{
	memcpy(this,defaults,sizeof(*this));
	
	gen_status_v = ONLINE;
	//rfb_size_v = SMALL;

	Rinternal = 10;
	V_Max = 0;
	I_Max = 0;
	E_Max = 0;
	Energy = -1;
	recalculate = true;
	margin = 1000;
	number_of_phases_out = 0;
	no_of_cycles = 0;
	deadband = 0;
	parasitic_power_draw = 0;
	additional_controls = AC_NONE;
	midpoint_temperature = 50;
	low_temperature = 0;
	high_temperature = 100;
	sensitivity = 0.5;
	
	Max_P = 0;//< maximum real power capacity in kW
    Min_P = 0;//< minimus real power capacity in kW
	
	//double Max_Q;//< maximum reactive power capacity in kVar
    //double Min_Q;//< minimus reactive power capacity in kVar
	Rated_kVA = 1; //< nominal capacity in kVA
	//double Rated_kV; //< nominal line-line voltage in kV
	
	efficiency =  0;
	base_efficiency = 0;
	Iteration_Toggle = false;

	E_Next = 0;
	connected = true;
	complex VA_Internal;
	
	/* TODO: set the context-free initial value of properties */
	return 1; /* return 1 on success, 0 on failure */
}

/* Object initialization is called once after all object have been created */
int battery::init(OBJECT *parent)
{
	OBJECT *obj = OBJECTHDR(this);

	static complex default_line123_voltage[1], default_line1_current[1];
	int i;

	// find parent meter, if not defined, use a default meter (using static variable 'default_meter')
	if (parent!=NULL && gl_object_isa(parent,"meter"))
	{
		// attach meter variables to each circuit
		struct {
			complex **var;
			char *varname;
		}
		map[] = {
		// local object name,	meter object name
			{&pCircuit_V,			"voltage_A"}, // assumes 2 and 3 follow immediately in memory
			{&pLine_I,				"current_A"}, // assumes 2 and 3(N) follow immediately in memory
			{&pPower,				"measured_power"},
		};
		/// @todo use triplex property mapping instead of assuming memory order for meter variables (residential, low priority) (ticket #139)
	
		NR_mode = get_bool(parent,"NR_mode");

		for (i=0; i<sizeof(map)/sizeof(map[0]); i++)
			*(map[i].var) = get_complex(parent,map[i].varname);

		node *par = OBJECTDATA(parent, node);
		number_of_phases_out = 0;
		if (par->has_phase(PHASE_A))
			number_of_phases_out += 1;
		if (par->has_phase(PHASE_B))
			number_of_phases_out += 1;
		if (par->has_phase(PHASE_C))
			number_of_phases_out += 1;
	}
	else if (parent!=NULL && gl_object_isa(parent,"triplex_meter"))
	{
		number_of_phases_out = 4; //Indicates it is a triplex node and should be handled differently
		struct {
			complex **var;
			char *varname;
		}
		map[] = {
			// local object name,	meter object name
			{&pCircuit_V,			"voltage_12"}, // assumes 1N and 2N follow immediately in memory
			{&pLine_I,				"current_1"}, // assumes 2 and 3(N) follow immediately in memory
			{&pLine12,				"current_12"}, // maps current load 1-2 onto triplex load
			{&pPower,				"measured_power"}, //looks at power being transferred through triplex meter
		};

		//Connect the NR mode indicator
		NR_mode = get_bool(parent,"NR_mode");

		// attach meter variables to each circuit
		for (i=0; i<sizeof(map)/sizeof(map[0]); i++)
		{
			if ((*(map[i].var) = get_complex(parent,map[i].varname))==NULL)
			{
				GL_THROW("%s (%s:%d) does not implement triplex_meter variable %s for %s (battery:%d)", 
				/*	TROUBLESHOOT
					The battery requires that the triplex_meter contains certain published properties in order to properly connect
					the battery to the triplex-meter.  If the triplex_meter does not contain those properties, GridLAB-D may
					suffer fatal pointer errors.  If you encounter this error, please report it to the developers, along with
					the version of GridLAB-D that raised this error.
				*/
				parent->name?parent->name:"unnamed object", parent->oclass->name, parent->id, map[i].varname, obj->name?obj->name:"unnamed", obj->id);
			}
		}
	}
	else if (parent != NULL && strcmp(parent->oclass->name,"inverter") == 0)
	{
		number_of_phases_out = 0; 
		struct {
			complex **var;
			char *varname;
		} 
	
		map[] = {
			// map the V & I from the inverter
			{&pCircuit_V,			"V_In"}, // assumes 2 and 3 follow immediately in memory
			{&pLine_I,				"I_In"}, // assumes 2 and 3(N) follow immediately in memory
		};

	 // construct circuit variable map to meter
		for (i=0; i<sizeof(map)/sizeof(map[0]); i++)
		{
			*(map[i].var) = get_complex(parent,map[i].varname);
		}
	}
	else if	((parent != NULL && strcmp(parent->oclass->name,"meter") != 0)||(parent != NULL && strcmp(parent->oclass->name,"triplex_meter") != 0)||(parent != NULL && strcmp(parent->oclass->name,"inverter") != 0))
	{
		throw("Battery must have a meter or triplex meter or inverter as it's parent");
		/*  TROUBLESHOOT
		Check the parent object of the inverter.  The battery is only able to be childed via a meter or 
		triplex meter when connecting into powerflow systems.  You can also choose to have no parent, in which
		case the battery will be a stand-alone application using default voltage values for solving purposes.
		*/
	}
	else
	{
		number_of_phases_out = 3;
		struct {
			complex **var;
			char *varname;
		}
		map[] = {
		// local object name,	meter object name
			{&pCircuit_V,			"voltage_A"}, // assumes 2 and 3 follow immediately in memory
			{&pLine_I,				"current_A"}, // assumes 2 and 3(N) follow immediately in memory
		};

		gl_warning("Battery:%d has no parent meter object defined; using static voltages", obj->id);

		// attach meter variables to each circuit in the default_meter
		*(map[0].var) = &default_line123_voltage[0];
		*(map[1].var) = &default_line1_current[0];

		// provide initial values for voltages
		default_line123_voltage[0] = complex(480,0);
		default_line123_voltage[1] = complex(480*cos(2*PI/3),480*sin(2*PI/3));
		default_line123_voltage[2] = complex(480*cos(-2*PI/3),480*sin(-2*PI/3));
	}

	if (gen_mode_v==GM_UNKNOWN)
	{
		throw("Generator (id:%d) generator_mode is not specified",obj->id);
	}
	else if (gen_mode_v == GM_VOLTAGE_CONTROLLED)
	{	
		GL_THROW("VOLTAGE_CONTROLLED generator_mode is not yet implemented.");
	}
		
	if (additional_controls == AC_LINEAR_TEMPERATURE)
	{
		static FINDLIST *climates = NULL;
		int not_found = 0;
		if (climates==NULL && not_found==0) 
		{
			climates = gl_find_objects(FL_NEW,FT_CLASS,SAME,"climate",FT_END);
			if (climates==NULL)
			{
				not_found = 1;
				gl_warning("battery: no climate data found, using static data");

				//default to mock data
				static double tout=59.0, solar=1000;
				pTout = &tout;
				pSolar = &solar;
			}
			else if (climates->hit_count>1)
			{
				gl_warning("battery: %d climates found, using first one defined", climates->hit_count);
			}
		}
		if (climates!=NULL)
		{
			if (climates->hit_count==0)
			{
				//default to mock data
				static double tout=59.0, solar=1000;
				pTout = &tout;
				pSolar = &solar;
			}
			else //climate data was found
			{
				OBJECT *clim = gl_find_next(climates,NULL);
				if (clim->rank<=obj->rank)
					gl_set_dependent(clim,obj);
				pTout = (double*)GETADDR(clim,gl_get_property(clim,"temperature"));
				pSolar = (double*)GETADDR(clim,gl_get_property(clim,"solar_flux"));
			}
		}
	}

	if (gen_status_v == OFFLINE)
	{
		//OBJECT *obj = OBJECTHDR(this);
		gl_warning("Generator (id:%d) is out of service!",obj->id);
	}
	else
	{
		switch(rfb_size_v)
		{
			case HOUSEHOLD:
				V_Max = 260;
				I_Max = 15;
				Max_P = V_Max.Mag()*I_Max.Mag(); 
				E_Max = 6*Max_P; //Wh		 -- 6 hours of maximum
				base_efficiency = 0.9; // roundtrip
				break;
			case SMALL:
				V_Max = 75.2; //V
				I_Max = 250; //A
				Max_P = 18800; //W
				E_Max = 160000; //Wh
				base_efficiency = 0.7; // roundtrip
				break;
			case MED_COMMERCIAL:
				V_Max = 115; //V
				I_Max = 435; //A
				Max_P = 50000; //W
				E_Max = 175000; //Wh
				base_efficiency = 0.8; // roundtrip
				break;
			case MED_HIGH_ENERGY:
				V_Max = 115; //V
				I_Max = 435; //A
				Max_P = 50000; //W
				E_Max = 400000; //Wh
				base_efficiency = 0.8; // roundtrip
				break;
			case LARGE:
				V_Max = 8000; //V  
				I_Max = 30; //A
				Max_P = V_Max.Mag()*I_Max.Mag(); 
				E_Max = 24*Max_P; //Wh		 
				base_efficiency = 0.9; // roundtrip
				break;
			default:
				if (V_Max == 0)
				{
					gl_warning("V_max was not given -- using default value (battery: %d)",obj->id);
					V_Max = 115; //V
				}
				if (I_Max == 0)
				{
					gl_warning("I_max was not given -- using default value (battery: %d)",obj->id);
					I_Max = 435; //A
				}
				if (Max_P == 0)
				{
					gl_warning("Max_P was not given -- using default value (battery: %d)",obj->id);
					Max_P = V_Max.Re()*I_Max.Re(); //W
				}
				if (E_Max == 0)
				{
					gl_warning("E_max was not given -- using default value (battery: %d)",obj->id);
					E_Max = 6*Max_P; //Wh
				}
				if (base_efficiency == 0)
				{
					gl_warning("base_efficiency was not given -- using default value (battery: %d)",obj->id);
					base_efficiency = 0.8; // roundtrip
				}
				break;
		}
	}
	if (power_set_high <= power_set_low && (gen_mode_v == GM_POWER_DRIVEN || gen_mode_v == GM_POWER_VOLTAGE_HYBRID))
		gl_warning("power_set_high is less than power_set_low -- oscillations will most likely occur");

	if (Energy < 0)
	{
		gl_warning("Initial Energy was not set, or set to a negative number.  Randomizing initial value.");
		Energy = gl_random_normal(RNGSTATE,E_Max/2,E_Max/20);
	}

	recalculate = true;
	power_transferred = 0;

	first_time_step = 0;

	return 1; /* return 1 on success, 0 on failure */
}


complex battery::calculate_v_terminal(complex v, complex i){
	return v - (i * Rinternal);
}


complex *battery::get_complex(OBJECT *obj, char *name)
{
	PROPERTY *p = gl_get_property(obj,name);
	if (p==NULL || p->ptype!=PT_complex)
		return NULL;
	return (complex*)GETADDR(obj,p);
}


TIMESTAMP battery::rfb_event_time(TIMESTAMP t0, complex power, double e){
	if((e < margin) && (power < 0)){
		return TS_NEVER;
	}
	if((e > E_Max - margin)&& (power > 0)){
		return TS_NEVER;
	}
	
	if(power < 0){ //dscharging
		double t1 = (e) / power.Re(); // time until depleted in hours
		t1 = t1 * 60 * 60; //time until depleted in seconds
		return t0 + (TIMESTAMP)(t1 * TS_SECOND);
	}else if(power > 0){ //charging
		double t1 = (E_Max - e) / power.Re(); // time until full in hours
		t1 = t1 * 60 * 60; //time until full in seconds
		return t0 + (TIMESTAMP)(t1 * TS_SECOND);
	}else{ //p ==0
		return TS_NEVER;
	}

}


double battery::calculate_efficiency(complex voltage, complex current){
	if(voltage.Mag() == 0 || current.Mag() == 0){
		return 1;
	}
	else if(current.Mag() < 5){
		return 0.95;
	}
	else if(current.Mag() < 10){
		return 0.9;
	}
	else if(current.Mag() < 20){
		return 0.8;
	}else{
		return 0.7;
	}

}



/* Presync is called when the clock needs to advance on the first top-down pass */
TIMESTAMP battery::presync(TIMESTAMP t0, TIMESTAMP t1)
{

	TIMESTAMP t2 = TS_NEVER;
	/* TODO: implement pre-topdown behavior */
	return t2; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

/* Sync is called when the clock needs to advance on the bottom-up pass */
TIMESTAMP battery::sync(TIMESTAMP t0, TIMESTAMP t1) 
{


	if (gen_mode_v == GM_POWER_DRIVEN || gen_mode_v == GM_POWER_VOLTAGE_HYBRID) 
	{
		if (number_of_phases_out == 3)
		{
			if (gen_mode_v == GM_POWER_VOLTAGE_HYBRID)
				GL_THROW("generator_mode POWER_VOLTAGE_HYBRID is not supported in 3-phase yet (only split phase is supported");

			complex volt[3];
			TIMESTAMP dt,t_energy_limit;

			for (int i=0;i<3;i++)
			{
				volt[i] = pCircuit_V[i];
			}

			if (first_time_step == 0)
			{
				dt = 0;
			}
			else if (prev_time == 0)
			{
				prev_time = t1;
				dt = 0;
			}
			else if (prev_time < t1)
			{
				dt = t1 - prev_time;
				prev_time = t1;
			}
			else
				dt = 0;
			
			if (prev_state == -1) //discharge
			{
				Energy = Energy - (1 / base_efficiency) * power_transferred * (double)dt / 3600;
				if (Energy < 0)
					Energy = 0;
			}
			else if (prev_state == 1) //charge
				Energy = Energy + base_efficiency * power_transferred * (double)dt / 3600;

			check_power = (*pPower).Mag();

			if (first_time_step > 0)
			{
				if (volt[0].Mag() > V_Max.Mag() || volt[1].Mag() > V_Max.Mag() || volt[2].Mag() > V_Max.Mag())
				{
					gl_warning("The voltages at the batteries meter are higher than rated. No power output.");
					battery_state = BS_WAITING;

					last_current[0].SetPolar(parasitic_power_draw/3/volt[0].Mag(),volt[0].Arg());
					last_current[1].SetPolar(parasitic_power_draw/3/volt[1].Mag(),volt[1].Arg());
					last_current[2].SetPolar(parasitic_power_draw/3/volt[2].Mag(),volt[2].Arg());
					
					pLine_I[0] += last_current[0];
					pLine_I[1] += last_current[1];
					pLine_I[2] += last_current[2];

					return TS_NEVER;
				}
				else if (check_power > power_set_high && Energy > 0) //discharge
				{
					prev_state = -1;
					battery_state = BS_DISCHARGING;

					double test_current = Max_P / (volt[0].Mag() + volt[1].Mag() + volt[2].Mag());
					if (test_current > I_Max.Mag()/3)
						test_current = I_Max.Mag()/3;

					last_current[0].SetPolar(-test_current + parasitic_power_draw/3/volt[0].Mag(),volt[0].Arg());
					last_current[1].SetPolar(-test_current + parasitic_power_draw/3/volt[1].Mag(),volt[1].Arg());
					last_current[2].SetPolar(-test_current + parasitic_power_draw/3/volt[2].Mag(),volt[2].Arg());
					
					pLine_I[0] += last_current[0];
					pLine_I[1] += last_current[1];
					pLine_I[2] += last_current[2];

					power_transferred = 0;

					for (int i=0;i<3;i++)
					{	
						power_transferred += last_current[i].Mag()*volt[i].Mag();
					}

					VA_Out = power_transferred;
					
					t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);
					if (t_energy_limit == 0)
						t_energy_limit = 1;

					return -(t1 + t_energy_limit);
				}
				else if (check_power < power_set_low && Energy < E_Max) //charge
				{
					prev_state = 1;
					battery_state = BS_CHARGING;

					double test_current = Max_P / (volt[0].Mag() + volt[1].Mag() + volt[2].Mag());
					if (test_current > I_Max.Mag()/3)
						test_current = I_Max.Mag()/3;

					last_current[0].SetPolar(test_current + parasitic_power_draw/3/volt[0].Mag(),volt[0].Arg());
					last_current[1].SetPolar(test_current + parasitic_power_draw/3/volt[1].Mag(),volt[1].Arg());
					last_current[2].SetPolar(test_current + parasitic_power_draw/3/volt[2].Mag(),volt[2].Arg());
					
					pLine_I[0] += last_current[0];
					pLine_I[1] += last_current[1];
					pLine_I[2] += last_current[2];

					power_transferred = 0;

					for (int i=0;i<3;i++)
					{	
						power_transferred += last_current[i].Mag()*volt[i].Mag();
					}
					
					VA_Out = power_transferred;

					t_energy_limit = TIMESTAMP(3600 * (E_Max - Energy) / power_transferred);
					if (t_energy_limit == 0)
						t_energy_limit = 1;

					return -(t1 + t_energy_limit);
				}
				else if ( (check_power < power_set_high - Max_P) && (prev_state == 1) && (Energy < E_Max) ) //keep charging until out of "deadband"
				{
					prev_state = 1; //charging
					battery_state = BS_CHARGING;

					double test_current = Max_P / (volt[0].Mag() + volt[1].Mag() + volt[2].Mag());
					if (test_current > I_Max.Mag()/3)
						test_current = I_Max.Mag()/3;

					last_current[0].SetPolar(test_current + parasitic_power_draw/3/volt[0].Mag(),volt[0].Arg());
					last_current[1].SetPolar(test_current + parasitic_power_draw/3/volt[1].Mag(),volt[1].Arg());
					last_current[2].SetPolar(test_current + parasitic_power_draw/3/volt[2].Mag(),volt[2].Arg());
					
					pLine_I[0] += last_current[0];
					pLine_I[1] += last_current[1];
					pLine_I[2] += last_current[2];

					power_transferred = 0;

					for (int i=0;i<3;i++)
					{	
						power_transferred += last_current[i].Mag()*volt[i].Mag();
					}
					
					VA_Out = power_transferred;

					t_energy_limit = TIMESTAMP(3600 * (E_Max - Energy) / power_transferred);
					if (t_energy_limit == 0)
						t_energy_limit = 1;

					return -(t1 + t_energy_limit);
				}
				else if ( (check_power > power_set_low + Max_P) && (prev_state == -1) && (Energy > 0) ) //keep discharging until out of "deadband"
				{
					prev_state = -1;
					battery_state = BS_DISCHARGING;
					
					double test_current = Max_P / (volt[0].Mag() + volt[1].Mag() + volt[2].Mag());
					if (test_current > I_Max.Mag()/3)
						test_current = I_Max.Mag()/3;

					last_current[0].SetPolar(-test_current + parasitic_power_draw/3/volt[0].Mag(),volt[0].Arg());
					last_current[1].SetPolar(-test_current + parasitic_power_draw/3/volt[1].Mag(),volt[1].Arg());
					last_current[2].SetPolar(-test_current + parasitic_power_draw/3/volt[2].Mag(),volt[2].Arg());
					
					pLine_I[0] += last_current[0];
					pLine_I[1] += last_current[1];
					pLine_I[2] += last_current[2];

					power_transferred = 0;

					for (int i=0;i<3;i++)
					{	
						power_transferred += last_current[i].Mag()*volt[i].Mag();
					}

					VA_Out = -power_transferred;
					
					t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);

					if (t_energy_limit == 0)
						t_energy_limit = 1;

					return -(t1 + t_energy_limit);
				}
				else
				{
					if (Energy <= 0)
						battery_state = BS_EMPTY;
					else if (Energy >= E_Max)
						battery_state = BS_FULL;
					else
						battery_state = BS_WAITING;

					last_current[0].SetPolar(parasitic_power_draw/3/volt[0].Mag(),volt[0].Arg());
					last_current[1].SetPolar(parasitic_power_draw/3/volt[1].Mag(),volt[1].Arg());
					last_current[2].SetPolar(parasitic_power_draw/3/volt[2].Mag(),volt[2].Arg());

					pLine_I[0] += last_current[0];
					pLine_I[1] += last_current[1];
					pLine_I[2] += last_current[2];

					prev_state = 0;
					power_transferred = 0;
					VA_Out = power_transferred;
					return TS_NEVER;
				}
			}
			else
			{
				if (Energy <= 0)
					battery_state = BS_EMPTY;
				else if (Energy >= E_Max)
					battery_state = BS_FULL;
				else
					battery_state = BS_WAITING;

				first_time_step = 1;
				return TS_NEVER;
			}
		} //End three phase GM_POWER_DRIVEN and HYBRID
		else if (number_of_phases_out == 4) // Split-phase
		{
			if (*NR_mode == false)
			{
				complex volt;
				TIMESTAMP dt,t_energy_limit;

				volt = pCircuit_V[0]; // 240-V circuit (we're assuming it can only be hooked here now

				if (first_time_step == 0)
				{
					dt = 0;
				}
				else if (prev_time == 0)
				{
					prev_time = t1;
					dt = 0;
				}
				else if (prev_time < t1)
				{
					dt = t1 - prev_time;
					prev_time = t1;
				}
				else
					dt = 0;
				
				if (prev_state == -1) //discharge
				{
					Energy = Energy - (1 / base_efficiency) * power_transferred * (double)dt / 3600;
					if (Energy < 0)
						Energy = 0;
				}
				else if (prev_state == 1) //charge
				{
					Energy = Energy + base_efficiency * power_transferred * (double)dt / 3600;
					if (Energy > E_Max)
						Energy = E_Max;
				}
				check_power = (*pPower).Mag();

				if (additional_controls == AC_LINEAR_TEMPERATURE)
				{
					double sens2 = (1 - sensitivity)/(-sensitivity);

					// high setpoint - high temperature
					double slope1_hi = power_set_high_highT / (high_temperature - midpoint_temperature * sens2);
					double yint1_hi = -midpoint_temperature * sens2 * slope1_hi;

					// high setpoint - low temperature
					double slope1_lo = (power_set_high - (slope1_hi * midpoint_temperature + yint1_hi)) / (low_temperature - midpoint_temperature);
					double yint1_lo = power_set_high - low_temperature * slope1_lo;

					// low setpoint - high temperature
					double slope2_hi = power_set_low_highT / (high_temperature - midpoint_temperature * sens2);
					double yint2_hi = -midpoint_temperature * sens2 * slope2_hi;

					// low setpoint - low temperature
					double slope2_lo = (power_set_low - (slope2_hi * midpoint_temperature + yint2_hi)) / (low_temperature - midpoint_temperature);
					double yint2_lo = power_set_low - low_temperature * slope2_lo;
				
					if (*pTout > midpoint_temperature)
					{
						check_power_high = slope1_hi * (*pTout) + yint1_hi;
						check_power_low = slope2_hi * (*pTout) + yint2_hi;
					}
					else
					{
						check_power_high = slope1_lo * (*pTout) + yint1_lo;
						check_power_low = slope2_lo * (*pTout) + yint2_lo;
					}
				}
				else
				{
					check_power_high = power_set_high;
					check_power_low = power_set_low;
				}

				if (first_time_step > 0)
				{
					if (volt.Mag() > V_Max.Mag() || volt.Mag()/240 < 0.9 || volt.Mag()/240 > 1.1)
					{
						gl_verbose("The voltages at the batteries meter are higher than rated, or outside of ANSI emergency specifications. No power output.");
						battery_state = BS_WAITING;

						last_current[0].SetPolar(parasitic_power_draw/volt.Mag(),volt.Arg());
						*pLine12 += last_current[0];

						

						return TS_NEVER;
					}
					// if the power flowing through the transformer has exceeded our setpoint, or the voltage has dropped below
					// our setpoint (if in HYBRID mode), then start discharging to help out
					else if ((check_power > check_power_high || (volt.Mag() < voltage_set_low && gen_mode_v == GM_POWER_VOLTAGE_HYBRID)) && Energy > 0) //start discharging
					{
						if (volt.Mag() > voltage_set_high) // conflicting states between power and voltage control
						{
							if (prev_state != 0)
								no_of_cycles += 1;

							last_current[0].SetPolar(parasitic_power_draw/volt.Mag(),volt.Arg());
							*pLine12 += last_current[0];
							

							VA_Out = power_transferred = 0;
							battery_state = BS_CONFLICTED;

							return TS_NEVER;
						}
						else
						{
							if (prev_state != -1)
								no_of_cycles +=1;

							prev_state = -1;  //discharging
							battery_state = BS_DISCHARGING;
							double power_desired = check_power - check_power_high;
							if (power_desired <= 0) // we're in voltage support mode
							{
								last_current[0].SetPolar(-I_Max.Mag(),volt.Arg());
								*pLine12 += last_current[0]; //generation, pure real power
								
							}
							else // We're load following
							{
								if (power_desired >= Max_P)
									power_desired = Max_P;

								double current_desired = -power_desired/volt.Mag();
								last_current[0].SetPolar(current_desired,volt.Arg());
								*pLine12 += last_current[0];
							}
							
							power_transferred = last_current[0].Mag()*volt.Mag();

							VA_Out = power_transferred;
							
							t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);
							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
					}
					// If the power has dropped below our setpoint, or voltage has risen to above our setpoint,
					// then start charging to help lower voltage and/or increase power through the transformer
					else if ((check_power < check_power_low || (gen_mode_v == GM_POWER_VOLTAGE_HYBRID && volt.Mag() > voltage_set_high)) && Energy < E_Max) //charge
					{
						if (volt.Mag() < voltage_set_low) // conflicting states between power and voltage control
						{
							if (prev_state != 0)
								no_of_cycles += 1;
							last_current[0].SetPolar(parasitic_power_draw/volt.Mag(),volt.Arg());
							*pLine12 += last_current[0];

							VA_Out = power_transferred = 0;
							battery_state = BS_CONFLICTED;

							return TS_NEVER;
						}
						else
						{
							if (prev_state != 1)
								no_of_cycles +=1;

							prev_state = 1;  //charging
							battery_state = BS_CHARGING;
							double power_desired = check_power_low - check_power;

							if (power_desired <= 0) // We're in voltage management mode
							{
								last_current[0].SetPolar(I_Max.Mag(),volt.Arg());
								*pLine12 += last_current[0]; //load, pure real power
								
							}
							else // We're in load tracking mode
							{
								if (power_desired >= Max_P)
									power_desired = Max_P;

								double current_desired = power_desired/volt.Mag();
								last_current[0].SetPolar(current_desired,volt.Arg());
								*pLine12 += last_current[0];
							}
			
							power_transferred = last_current[0].Mag()*volt.Mag();
							
							VA_Out = power_transferred;

							t_energy_limit = TIMESTAMP(3600 * (E_Max - Energy) / power_transferred);
							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
					}
					//keep charging until out of "deadband"
					else if ((check_power < check_power_low || (gen_mode_v == GM_POWER_VOLTAGE_HYBRID && volt.Mag() > voltage_set_high - deadband))&& prev_state == 1 && Energy < E_Max) 
					{
						if (volt.Mag() < voltage_set_low) // conflicting states between power and voltage control
						{
							if (prev_state != 0)
								no_of_cycles += 1;

							last_current[0].SetPolar(parasitic_power_draw/volt.Mag(),volt.Arg());
							*pLine12 += last_current[0];

							VA_Out = power_transferred = 0;
							battery_state = BS_CONFLICTED;

							return TS_NEVER;
						}
						else
						{
							if (prev_state != 1)
								no_of_cycles +=1;

							prev_state = 1; //charging
							battery_state = BS_CHARGING;

							double power_desired = check_power_low - check_power;

							if (power_desired <= 0) // We're in voltage management mode
							{
								last_current[0].SetPolar(I_Max.Mag(),volt.Arg());
								*pLine12 += last_current[0]; //load, pure real power
								
							}
							else // We're in load tracking mode
							{
								if (power_desired >= Max_P)
									power_desired = Max_P;

								double current_desired = power_desired/volt.Mag();
								last_current[0].SetPolar(current_desired, volt.Arg());
								*pLine12 += last_current[0];
							}
			
							power_transferred = last_current[0].Mag()*volt.Mag();
							
							VA_Out = power_transferred;

							t_energy_limit = TIMESTAMP(3600 * (E_Max - Energy) / power_transferred);
							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
					}
					//keep discharging until out of "deadband"
					else if ((check_power > check_power_high || (gen_mode_v == GM_POWER_VOLTAGE_HYBRID && volt.Mag() < voltage_set_low + deadband)) && prev_state == -1 && Energy > 0) 
					{
						if (volt.Mag() > voltage_set_high) // conflicting states between power and voltage control
						{
							if (prev_state != 0)
								no_of_cycles += 1;

							last_current[0].SetPolar(parasitic_power_draw/volt.Mag(),volt.Arg());
							*pLine12 += last_current[0];

							VA_Out = power_transferred = 0;
							battery_state = BS_CONFLICTED;

							return TS_NEVER;
						}
						else
						{
							if (prev_state != -1)
								no_of_cycles +=1;

							prev_state = -1; //discharging
							battery_state = BS_DISCHARGING;

							double power_desired = check_power - check_power_high;
							if (power_desired <= 0) // we're in voltage support mode
							{
								last_current[0].SetPolar(-I_Max.Mag(),volt.Arg());
								*pLine12 += last_current[0]; //generation, pure real power
								
							}
							else // We're load following
							{
								if (power_desired >= Max_P)
									power_desired = Max_P;

								double current_desired = -power_desired/volt.Mag();
								last_current[0].SetPolar(current_desired,volt.Arg());
								*pLine12 += last_current[0];						
							}

							power_transferred = last_current[0].Mag()*volt.Mag();

							VA_Out = power_transferred;
							
							t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);

							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
					}
					else
					{
						if (prev_state != 0)
							no_of_cycles +=1;

						last_current[0].SetPolar(parasitic_power_draw/volt.Mag(),volt.Arg());
						*pLine12 += last_current[0];

						prev_state = 0;
						if (Energy <= 0)
							battery_state = BS_EMPTY;
						else if (Energy >= E_Max)
							battery_state = BS_FULL;
						else
							battery_state = BS_WAITING;

						power_transferred = 0;
						VA_Out = power_transferred;
						return TS_NEVER;
					}
				}
				else
				{
					if (Energy <= 0)
						battery_state = BS_EMPTY;
					else if (Energy >= E_Max)
						battery_state = BS_FULL;
					else
						battery_state = BS_WAITING;

					first_time_step = 1;
					return TS_NEVER;
				}
			} // NR_cycle
		} //End split phase GM_POWER_DRIVEN and HYBRID
	}// End GM_POWER_DRIVEN and HYBRID controls
	else if (gen_mode_v == GM_CONSTANT_PQ)
	{
		if (number_of_phases_out == 3) //Three phase meter
		{
			if (*NR_mode == false)
			{
				complex volt[3];
				double high_voltage = 0, low_voltage = 999999999;
				TIMESTAMP dt,t_energy_limit;

				for (int jj=0; jj<3; jj++)
				{
					volt[jj] = pCircuit_V[jj]; // 240-V circuit (we're assuming it can only be hooked here now)
					if (volt[jj].Mag() > high_voltage)
						high_voltage = volt[jj].Mag();
					if (volt[jj].Mag() < low_voltage)
						low_voltage = volt[jj].Mag();
				}
				if (first_time_step == 0)
				{
					dt = 0;
				}
				else if (prev_time == 0)
				{
					prev_time = t1;
					dt = 0;
				}
				else if (prev_time < t1)
				{
					dt = t1 - prev_time;
					prev_time = t1;
				}
				else
					dt = 0;
				
				if (prev_state == -1) //discharge
				{
					Energy = Energy - (1 / base_efficiency) * power_transferred * (double)dt / 3600;
					if (Energy < 0)
						Energy = 0;
				}
				else if (prev_state == 1) //charge
				{
					Energy = Energy + base_efficiency * power_transferred * (double)dt / 3600;
					if (Energy > E_Max)
						Energy = E_Max;
				}

				if (first_time_step > 0)
				{
					if (high_voltage > V_Max.Mag())
					{
						gl_verbose("The voltages at the battery meter are higher than rated. No power output.");
						battery_state = BS_WAITING;
						prev_state = 0;

						for (int jj=0; jj<3; jj++)
						{
							last_current[jj].SetPolar(parasitic_power_draw/3/volt[jj].Mag(),volt[jj].Arg());
							pLine_I[jj] += last_current[jj];
						}
						return TS_NEVER;
					}
					else // We're within the voltage limits, so follow the schedule
					{
						double power_desired;
						if (abs(B_scheduled_power) < abs(Max_P))
							 power_desired = B_scheduled_power;
						else
							power_desired = Max_P;

						if (power_desired < 0.0 && Energy < E_Max)
						{
							battery_state = BS_CHARGING;
							prev_state = 1;

							VA_Out = power_transferred = 0;
							for (int jj=0; jj<3; jj++)
							{
								last_current[jj].SetPolar((-power_desired + parasitic_power_draw)/3/volt[jj].Mag(),volt[jj].Arg());
								pLine_I[jj] += last_current[jj];
								VA_Out += last_current[jj].Mag()*volt[jj].Mag();
							}					
							power_transferred = VA_Out.Mag();

							t_energy_limit = TIMESTAMP(3600 * (E_Max - Energy) / power_transferred);

							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
						else if (power_desired > 0.0 && Energy > 0.0)
						{
							battery_state = BS_DISCHARGING;
							prev_state = -1;

							VA_Out = power_transferred = 0;
							for (int jj=0; jj<3; jj++)
							{
								last_current[jj].SetPolar((-power_desired + parasitic_power_draw)/3/volt[jj].Mag(),volt[jj].Arg());
								pLine_I[jj] += last_current[jj];
								VA_Out += last_current[jj].Mag()*volt[jj].Mag();
							}					
							power_transferred = VA_Out.Mag();

							t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);

							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
						else if (Energy <= 0.0)
						{
							battery_state = BS_EMPTY;
							prev_state = 0;

							VA_Out = power_transferred = 0.0;
							last_current[0] = last_current[1] = last_current[2] = complex(0,0);
							return TS_NEVER;
						}
						else if (Energy >= E_Max)
						{
							battery_state = BS_FULL;
							prev_state = -1;

							VA_Out = power_transferred = 0;
							for (int jj=0; jj<3; jj++)
							{
								last_current[jj].SetPolar((parasitic_power_draw)/3/volt[jj].Mag(),volt[jj].Arg());
								pLine_I[jj] += last_current[jj];
								VA_Out += last_current[jj].Mag()*volt[jj].Mag();
							}					
							power_transferred = VA_Out.Mag();
							t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);

							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
						else
						{
							battery_state = BS_WAITING;
							prev_state = -1;

							VA_Out = power_transferred = 0;
							for (int jj=0; jj<3; jj++)
							{
								last_current[jj].SetPolar((parasitic_power_draw)/3/volt[jj].Mag(),volt[jj].Arg());
								pLine_I[jj] += last_current[jj];
								VA_Out += last_current[jj].Mag()*volt[jj].Mag();
							}					
							power_transferred = VA_Out.Mag();

							t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);

							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
					}
				}
				else
				{
					if (Energy <= 0)
					{
						battery_state = BS_EMPTY;
						prev_state = 0;
					}
					else if (Energy >= E_Max)
					{
						battery_state = BS_FULL;
						prev_state = -1;
					}
					else
					{
						battery_state = BS_WAITING;
						prev_state = 0;
					}

					first_time_step = 1;
					return TS_NEVER;
				}
			}// End NR_Cycle
		}// End 3-phase meter
		else if (number_of_phases_out == 4) // Split-phase
		{
			if (*NR_mode == false)
			{
				complex volt;
				TIMESTAMP dt,t_energy_limit;

				volt = pCircuit_V[0]; // 240-V circuit (we're assuming it can only be hooked here now)

				if (first_time_step == 0)
				{
					dt = 0;
				}
				else if (prev_time == 0)
				{
					prev_time = t1;
					dt = 0;
				}
				else if (prev_time < t1)
				{
					dt = t1 - prev_time;
					prev_time = t1;
				}
				else
					dt = 0;
				
				if (prev_state == -1) //discharge
				{
					Energy = Energy - (1 / base_efficiency) * power_transferred * (double)dt / 3600;
					if (Energy < 0)
						Energy = 0;
				}
				else if (prev_state == 1) //charge
				{
					Energy = Energy + base_efficiency * power_transferred * (double)dt / 3600;
					if (Energy > E_Max)
						Energy = E_Max;
				}

				if (first_time_step > 0)
				{
					if (volt.Mag() > V_Max.Mag() || volt.Mag()/240 < 0.9 || volt.Mag()/240 > 1.1)
					{
						gl_verbose("The voltages at the battery meter are higher than rated, or outside of ANSI emergency specifications. No power output.");
						battery_state = BS_WAITING;
						prev_state = -1;

						last_current[0].SetPolar(parasitic_power_draw/volt.Mag(),volt.Arg());
						*pLine12 += last_current[0];

						return TS_NEVER;
					}
					else 
					{
						double power_desired;
						if (abs(B_scheduled_power) < abs(Max_P))
							 power_desired = B_scheduled_power;
						else
							power_desired = Max_P;

						if (power_desired < 0.0 && Energy < E_Max)
						{
							battery_state = BS_CHARGING;
							prev_state = 1;

							double current_desired = -(power_desired - parasitic_power_draw) / volt.Mag();
							last_current[0].SetPolar(current_desired,volt.Arg());
							*pLine12 += last_current[0];

							VA_Out = power_transferred = last_current[0].Mag()*volt.Mag();

							t_energy_limit = TIMESTAMP(3600 * (E_Max - Energy) / power_transferred);

							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
						else if (power_desired > 0.0 && Energy > 0.0)
						{
							battery_state = BS_DISCHARGING;
							prev_state = -1;

							double current_desired = -(power_desired - parasitic_power_draw) / volt.Mag();
							last_current[0].SetPolar(current_desired,volt.Arg());
							*pLine12 += last_current[0];

							VA_Out = power_transferred = last_current[0].Mag()*volt.Mag();

							t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);

							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
						else if (Energy <= 0.0)
						{
							battery_state = BS_EMPTY;
							prev_state = 0;

							VA_Out = power_transferred = 0.0;
							last_current[0] = complex(0,0);
							return TS_NEVER;
						}
						else if (Energy >= E_Max)
						{
							battery_state = BS_FULL;
							prev_state = -1;

							double current_desired = (parasitic_power_draw) / volt.Mag();
							last_current[0].SetPolar(current_desired,volt.Arg());
							*pLine12 += last_current[0];

							VA_Out = power_transferred = last_current[0].Mag()*volt.Mag();

							t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);

							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
						else
						{
							battery_state = BS_WAITING;
							prev_state = -1;

							double current_desired = (parasitic_power_draw) / volt.Mag();
							last_current[0].SetPolar(current_desired,volt.Arg());
							*pLine12 += last_current[0];

							VA_Out = power_transferred = last_current[0].Mag()*volt.Mag();

							t_energy_limit = TIMESTAMP(3600 * Energy / power_transferred);

							if (t_energy_limit == 0)
								t_energy_limit = 1;

							return -(t1 + t_energy_limit);
						}
					}
				}
				else
				{
					if (Energy <= 0)
					{
						battery_state = BS_EMPTY;
						prev_state = 0;
					}
					else if (Energy >= E_Max)
					{
						battery_state = BS_FULL;
						prev_state = -1;
					}
					else
					{
						battery_state = BS_WAITING;
						prev_state = 0;
					}

					first_time_step = 1;
					return TS_NEVER;
				}
			}// End NR_Cycle
		}// End Split-phase
	}// End Constant_PQ
	else
	{
		//gather V_Out
		//gather I_Out
		//gather P_Out
		////gl_verbose("battery sync: entered");
		V_Out = pCircuit_V[0];
		I_Out = pLine_I[0];

		//gl_verbose("battery sync: V_Out from parent is: (%f , %f)", V_Out.Re(), V_Out.Im());
		//gl_verbose("battery sync: I_Out from parent is: (%f , %f)", I_Out.Re(), V_Out.Im());


		V_In = V_Out;

		V_Internal = calculate_v_terminal(V_Out, I_Out);
		//V_Out = V_internal;

		//finds the CURRENT efficiency
		efficiency = base_efficiency * calculate_efficiency(V_Out, I_Out);

		//gl_verbose("battery sync: base_efficiency is: %f", base_efficiency);
		//gl_verbose("battery sync: efficiency is: %f", efficiency);

		if (I_Out > 0){ // charging
			I_Internal = I_Out * efficiency; //actual stored current is less than what was put in
		}

		if(I_Out < 0){ // discharging
			I_Internal = I_Out / efficiency; //actual released current is more than what is obtained by parent
		}


		//gl_verbose("battery sync: V_In to push is: (%f , %f)", V_In.Re(), V_In.Im());
		//gl_verbose("battery sync: V_Internal to push is: (%f , %f)", V_Internal.Re(), V_Internal.Im());
		//gl_verbose("battery sync: I_Internal to push is: (%f , %f)", I_Internal.Re(), I_Internal.Im());

		VA_Out = V_Out * (~I_Out);
		VA_Internal = V_Internal * I_Internal;  // charging rate is slower than termainl voltage suggests, similarly,
											//discharge rate is faster than the terminal voltage suggests
			
		//gl_verbose("battery sync: VA_Out from parent is: (%f , %f)", VA_Out.Re(), VA_Out.Im());
		//gl_verbose("battery sync: VA_Internal calculated is: (%f , %f)", VA_Internal.Re(), VA_Internal.Im());
		

		if(!recalculate){//if forced recalculate is false, check where the time is
		//gl_verbose("battery sync: don't recalculate");
			if(t0 == prev_time){
				//gl_verbose("battery sync: reached expected time, set energy and don't recalculate");
				Energy = E_Next; // we're all set this time around, just set the energy level onward
				//gl_verbose("battery sync: Energy level is %f", Energy);
				//gl_verbose("battery sync: V_In to push is: (%f , %f)", V_In.Re(), V_In.Im());
				//gl_verbose("battery sync: I_In to push is: (%f , %f)", I_In.Re(), I_In.Im());
				recalculate = true; //recalculate the next time around
				
				return battery::sync(t0, t1);
			}else{
				//gl_verbose("battery sync: Didn't reach expected time, force recalculate");
				//gl_verbose("battery sync: V_In to push is: (%f , %f)", V_In.Re(), V_In.Im());
				//gl_verbose("battery sync: I_In to push is: (%f , %f)", I_In.Re(), I_In.Im());
				Energy = E_Next;
				//TIMESTAMP prev_time2 = prev_time;
				//prev_time = t0;
				recalculate = true; //set forced recalculate to true becuase we've reached an unexpected time
				//return battery::sync(prev_time2, t0);
				return battery::sync(prev_time, t1);
			}
			
		}else{
			//////////////////// need this in hours //////////////////
			//TIMESTAMP t2 = t1 - t0;
			//gl_verbose("battery sync: recalculate!");
			//gl_verbose("battery sync: energy level pre changes is: %f", Energy);
			//gl_verbose("battery sync: energy level to go to next is: %f", E_Next);

			///need to use timestamp_to_hours from the core, not here, how to reference core?
			//double t2 = (timestamp_to_hours((TIMESTAMP)t1) - timestamp_to_hours((TIMESTAMP)t0));
			double t2 = (gl_tohours((TIMESTAMP)t1) - gl_tohours((TIMESTAMP)t0));

			if(abs((double)V_Out.Re()) > abs((double)V_Max.Re())){
				//gl_verbose("battery sync: V_Out exceeded allowable V_Out, setting to max");
				V_Out = V_Max;
				V_In = V_Out;
				V_Internal = V_Out - (I_Out * Rinternal);
			}

			if(abs((double)I_Out.Re()) > abs((double)I_Max.Re())){
				//gl_verbose("battery sync: I_Out exceeded allowable I_Out, setting to max");
				I_Out = I_Max;
			}

			if(abs((double)VA_Out.Re()) > abs((double)Max_P)){
				//gl_verbose("battery sync: VA_Out exceeded allowable VA_Out, setting to max");
				VA_Out = complex(Max_P , 0);
				VA_Internal = VA_Out - (I_Out * I_Out * Rinternal);
			}



		prev_time = t1;

		if(VA_Out < 0){ //discharging
			//gl_verbose("battery sync: discharging");
			if(Energy == 0 || Energy <= margin){ 
				//gl_verbose("battery sync: battery is empty!");
				if(connected){
					//gl_verbose("battery sync: empty BUT it is connected, passing request onward");
					I_In = I_Max + complex(abs(I_Out.Re()), abs(I_Out.Im())); //power was asked for to discharge but battery is empty, forward request along the line
					I_Prev = I_Max / efficiency;
					//Get as much as you can from the microturbine --> the load asked for as well as the max
					recalculate = false;
					E_Next = Energy + (((I_In - complex(abs(I_Out.Re()), abs(I_Out.Im()))) * V_Internal / efficiency) * t2).Re();  // the energy level at t1
					TIMESTAMP t3 = rfb_event_time(t0, (I_In - complex(abs(I_Out.Re()), abs(I_Out.Im()))) * V_Internal / efficiency, Energy);
					return t3;
				}
				else{
					//gl_verbose("battery sync: battery is empty with nothing connected!  drop request!");
					I_In = 0;
					I_Prev = 0;
					V_In = V_Out;
					VA_Out = 0;
					E_Next = 0;
					recalculate = false;
					return TS_NEVER;
				}
			}

			if((Energy + (V_Internal * I_Prev.Re()).Re() * t2) <= margin){ //headed to empty
				//gl_verbose("battery sync: battery is headed to empty");
				if(connected){
					//gl_verbose("battery sync: BUT battery is connected, so pass request onward");
					I_In = I_Max + complex(abs(I_Out.Re()), abs(I_Out.Im())); //this won't let the battery go empty... change course 
					I_Prev = I_Max / efficiency;
					//if the battery is connected to something which serves the load and charges the battery
					E_Next = margin;
					recalculate = false;
					TIMESTAMP t3 = rfb_event_time(t0, (I_In - complex(abs(I_Out.Re()), abs(I_Out.Im()))) * V_Internal / efficiency, Energy);
					return t3;
				}else{
					//gl_verbose("battery sync: battery is about to be empty with nothing connected!!");
					TIMESTAMP t3 = rfb_event_time(t0, VA_Internal, Energy);
					E_Next = 0; //expecting return when time is empty
					I_In = 0; 
					I_Prev = 0;
					recalculate = false;
					return t3;
				}
			}else{ // doing fine
				//gl_verbose("battery sync: battery is not empty, demand supplied from the battery");
				E_Next = Energy + (VA_Internal.Re() * t2);
				I_In = 0; //nothign asked for
				I_Prev = 0;
				recalculate = false;
				TIMESTAMP t3 = rfb_event_time(t0, VA_Internal, Energy);
				return t3;
			}
		}else if (VA_Out > 0){ //charging
			if(Energy >= (E_Max - margin)){
				//gl_verbose("battery sync: battery is full!");
				if(connected){
					//attempt to let other items serve the load if the battery is full instead of draining the battery
					//gl_verbose("battery sync: battery is full and connected, passing the power request onward");
					E_Next = Energy;
					I_In = I_Out = 0; //drop the request to charge
					I_Prev = 0;
					recalculate = false;
					return TS_NEVER;
				}else{
					//gl_verbose("battery sync: battery is full, and charging");
					I_In = I_Out = 0; //can't charge any more... drop the current somehow?
					I_Prev = 0;
					V_Out = V_Out; // don't drop V_Out in this case
					VA_Out = 0;
					E_Next = Energy;
					return TS_NEVER;
				}
			}

			if(Energy + ((V_Internal * I_Prev.Re()) * efficiency * t2).Re() >= (E_Max - margin)){ //if it is this far, it is charging at max
				//gl_verbose("battery sync: battery is about to be full");
				TIMESTAMP t3 = rfb_event_time(t0, VA_Internal, Energy);
				I_In = 0;
				I_Prev = 0;
				E_Next = E_Max - margin;
				recalculate = false;
				return t3;
			}else{
				if(connected){
					//gl_verbose("battery sync: battery is charging but not yet full, connected");
					//if it is connected, use whatever is connected to help it charge;
					I_In = I_Max - I_Out; // total current in is now I_Max
					I_Prev = I_Max * efficiency;
					E_Next = Energy + ((I_Max * V_Internal) * efficiency * t2).Re();
					recalculate = false;
					TIMESTAMP t3 = rfb_event_time(t0, (I_Max * V_Internal * efficiency), Energy);
					return t3;
				}else{
					//gl_verbose("battery sync: battery is charging but not yet full, not connected");
					I_In = 0;
					I_Prev = 0;
					E_Next = Energy + (VA_Internal * t2).Re();
					recalculate = false;
					TIMESTAMP t3 = rfb_event_time(t0, VA_Internal, Energy);
					return t3;
				}
			}
		}else{// VA_Out = 0
			//gl_verbose("battery sync: battery is not charging or discharging");
			recalculate = false;
			I_In = 0;
			I_Prev = 0;
			E_Next = Energy;
			return TS_NEVER;
		}

		}
	}

	return TS_NEVER;
}

/* Postsync is called when the clock needs to advance on the second top-down pass */
TIMESTAMP battery::postsync(TIMESTAMP t0, TIMESTAMP t1)
{

	TIMESTAMP result;
	if (*NR_mode == false)
	{
		Iteration_Toggle = !Iteration_Toggle;
		if (number_of_phases_out == 3)
		{
			pLine_I[0] -= last_current[0];
			pLine_I[1] -= last_current[1];
			pLine_I[2] -= last_current[2];
		}
		else if (number_of_phases_out == 4)
		{
			complex temp;
			temp = *pLine12;
			temp -= last_current[0];
			*pLine12 = temp;
		}
		result = TS_NEVER;
	}
	else
		result = TS_NEVER;

	if (Iteration_Toggle == true)
		return result;
	else
		return TS_NEVER;
}

bool *battery::get_bool(OBJECT *obj, char *name)
{
	PROPERTY *p = gl_get_property(obj,name);
	if (p==NULL || p->ptype!=PT_bool)
		return NULL;
	return (bool*)GETADDR(obj,p);
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int create_battery(OBJECT **obj, OBJECT *parent) 
{
	try 
	{
		*obj = gl_create_object(battery::oclass);
		if (*obj!=NULL)
		{
			battery *my = OBJECTDATA(*obj,battery);
			gl_set_parent(*obj,parent);
			return my->create();
		}
		else
			return 0;
	} 
	CREATE_CATCHALL(battery);
}

EXPORT int init_battery(OBJECT *obj, OBJECT *parent) 
{
	try 
	{
		if (obj!=NULL)
			return OBJECTDATA(obj,battery)->init(parent);
		else
			return 0;
	}
	INIT_CATCHALL(battery);
}

EXPORT TIMESTAMP sync_battery(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	TIMESTAMP t2 = TS_NEVER;
	battery *my = OBJECTDATA(obj,battery);
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
	}
	SYNC_CATCHALL(battery);
	return t2;
}
