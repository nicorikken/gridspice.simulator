/* $Id: wholesale.h 683 2008-06-18 20:16:29Z d3g637 $ */

#ifndef _wholesale_H
#define _wholesale_H

#include <stdarg.h>
#include "gridlabd.h"
#include "matrix.h"
#include "libopf.h"

#include <vector>
using std::vector;

#include <string>
using std::string;

#ifndef ATTR_NUM
#define BUS_ATTR 13
#define	BUS_ATTR_FULL 17
#define GEN_ATTR 21
#define	GEN_ATTR_FULL 25
#define BRANCH_ATTR 13
#define BRANCH_ATTR_FULL 21
#define GENCOST_ATTR 4
#define AREA_ATTR 2
#define BASEMVA_ATTR 1
#endif

/*** DO NOT DELETE THE NEXT LINE ***/
//NEWCLASSINC
inline mxArray* initArray(double rdata[], int nRow, int nColumn);
inline double*	getArray(mxArray* X);

int solver_matpower();

inline vector<string> split(const string s, char c);

void setObjectValue_Double(OBJECT* obj, char* Property, double value);

void setObjectValue_Double2Complex_inDegree(OBJECT* obj, char* Property, double Mag, double Ang);

void setObjectValue_Double2Complex(OBJECT* obj, char* Property, double Re, double Im);

void setObjectValue_Complex(OBJECT* obj, char* Property, complex val);

void setObjectValue_Char(OBJECT* obj, char* Property, char* value);

/* optional exports */
#ifdef OPTIONAL

/* TODO: define this function to enable checks routine */
EXPORT int check(void);

/* TODO: define this function to allow direct import of models */
EXPORT int import_file(char *filename);

/* TODO: define this function to allow direct export of models */
EXPORT int export_file(char *filename);

/* TODO: define this function to allow export of KML data for a single object */
EXPORT int kmldump(FILE *fp, OBJECT *obj);
#endif

#endif