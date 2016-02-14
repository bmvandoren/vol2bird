/* --------------------------------------------------------------------
Copyright (C) 2011 Swedish Meteorological and Hydrological Institute, SMHI,

This file is part of beamb.

beamb is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

beamb is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with beamb.  If not, see <http://www.gnu.org/licenses/>.
------------------------------------------------------------------------*/
/**
 * Python API to the vol2bird functions
 * @file
 * @author Anders Henja (Swedish Meteorological and Hydrological Institute, SMHI)
 * @date 2011-11-14
 */
#include "Python.h"
#include <math.h>
#include <stdio.h>
#include <string.h>


#define PYVOL2BIRD_MODULE   /**< to get correct part in pyvol2bird */
#include "pyvol2bird.h"

#include "pyverticalprofile.h"
#include "pypolarvolume.h"
#include "pyrave_debug.h"
#include "rave_alloc.h"

/**
 * Debug this module
 */
PYRAVE_DEBUG_MODULE("_vol2bird");

/**
 * Sets a python exception and goto tag
 */
#define raiseException_gotoTag(tag, type, msg) \
{PyErr_SetString(type, msg); goto tag;}

/**
 * Sets python exception and returns NULL
 */
#define raiseException_returnNULL(type, msg) \
{PyErr_SetString(type, msg); return NULL;}

/**
 * Error object for reporting errors to the python interpreeter
 */
static PyObject *ErrorObject;

/// --------------------------------------------------------------------
/// Vol2Bird
/// --------------------------------------------------------------------
/*@{ Vol2Bird */
/**
 * Returns the native Vol2Bird_t instance.
 * @param[in] vol - the python Vol2Bird instance
 * @returns the native BeamBlockage_t instance.
 */
static vol2bird_t*
PyVol2Bird_GetNative(PyVol2Bird* v2b)
{
  RAVE_ASSERT((v2b != NULL), "v2b == NULL");
  return RAVE_OBJECT_COPY(v2b->v2b);
}

static PyVol2Bird* PyVol2Bird_New(PolarVolume_t* volume)
{
  PyVol2Bird* result = NULL;
  vol2bird_t* alldata = NULL;

  int initSuccessful = vol2birdSetUp(volume, alldata) == 0;
//  cp = RAVE_MALLOC(sizeof(vol2bird_t)); // REPLACE WITH ALLOC METHOD
  if (initSuccessful == FALSE) {
     raiseException_returnNULL(PyExc_ValueError, "vol2birdSetUp did not complete successfully.");
  }
  if (alldata == NULL) {
    RAVE_CRITICAL0("Failed to allocate memory for Vol2Bird.");
    raiseException_returnNULL(PyExc_MemoryError, "Failed to allocate memory for Vol2Bird.");
  }
  result = PyObject_NEW(PyVol2Bird, &PyVol2Bird_Type);
  if (result != NULL) {
    result->v2b = alldata;
  }
  return result;
}

/**
 * Deallocates the beam blockage
 * @param[in] obj the object to deallocate.
 */
static void _vol2bird_dealloc(PyVol2Bird* obj)
{
  /*Nothing yet*/
  if (obj == NULL) {
    return;
  }
  //PYRAVE_DEBUG_OBJECT_DESTROYED;

// tear down vol2bird, give memory back
// fairly easy to remove cfg from vol2birdTearDown, to have a single argument destrocutor
// suggested replacement: vol2birdTearDown(cfg, obj->v2b);
//  RAVE_FREE(obj->v2b); // REPLACE WITH FREE METHOD
  vol2birdTearDown(obj->v2b);
  PyObject_Del(obj);
}

/**
 * Creates a new instance of the vol2bird
 * @param[in] self this instance.
 * @param[in] args arguments for creation or a beam blockage
 * @return the object on success, otherwise NULL
 */
static PyObject* _pyvol2bird_new(PyObject* self, PyObject* args)
{
  PyObject* pyin = NULL;
  if (!PyArg_ParseTuple(args, "O", &pyin)) {
    return NULL;
  }

  if (!PyPolarVolume_Check(pyin)) {
    raiseException_returnNULL(PyExc_ValueError, "First argument should be a Polar Scan");
  }
  //this gives an error at compile: pvol not known
  //return (PyObject*)PyVol2Bird_New((PyPolarVolume*)pyin)->pvol);
  
  return (PyObject*)PyVol2Bird_New(PyPolarVolume_GetNative((PyPolarVolume*)pyin));
}

/**
 * Returns the blockage for the provided scan given gaussian limit.
 * @param[in] self - self
 * @param[in] args - the arguments (PyPolarVolume, double (Limit of Gaussian approximation of main lobe))
 * @return the PyRaveField on success otherwise NULL
 */
static PyObject* _pyvol2bird_vol2bird(PyVol2Bird* self, PyObject* args)
{
  PyObject* pyin = NULL;
  double dBlim = 0;
  VerticalProfile_t* vp = NULL;
  PyObject* result = NULL;

  if (!PyArg_ParseTuple(args, "Od", &pyin, &dBlim)) {
    return NULL;
  }

  if (!PyPolarVolume_Check(pyin)) {
    raiseException_returnNULL(PyExc_ValueError, "First argument should be a Polar Scan");
  }

  /* FIX ME vp = vol2bird(self->v2b, ((PyPolarVolume*)pyin)->pvol, dBlim);*/
  vol2birdCalcProfiles(self->v2b);
  // TO ADD: constructor for vp
  if (vp != NULL) {
    result = (PyObject*)PyVerticalProfile_New(vp);
  }
  RAVE_OBJECT_RELEASE(vp);
  return result;
}

/**
 * All methods a ropo generator can have
 */
static struct PyMethodDef _pyvol2bird_methods[] =
{
  {"constants_nGatesCellMin", NULL},
  {"constants_cellDbzMin", NULL},
  {"vol2bird", (PyCFunction)_pyvol2bird_vol2bird, 1},
  {NULL, NULL} /* sentinel */
};

/**
 * Returns the specified attribute in the beam blockage
 */
static PyObject* _pyvol2bird_getattr(PyVol2Bird* self, char* name)
{
  PyObject* res = NULL;

  if (strcmp("constants_nGatesCellMin", name) == 0) {
    return PyInt_FromLong(self->v2b->constants.nGatesCellMin);
  } else if(strcmp("constants_cellDbzMin", name) == 0) {
    return PyFloat_FromDouble(self->v2b->constants.cellDbzMin);
  }

  res = Py_FindMethod(_pyvol2bird_methods, (PyObject*) self, name);
  if (res)
    return res;

  PyErr_Clear();
  PyErr_SetString(PyExc_AttributeError, name);
  return NULL;
}

/**
 * Returns the specified attribute in the polar volume
 */
static int _pyvol2bird_setattr(PyVol2Bird* self, char* name, PyObject* val)
{
  int result = -1;
  if (name == NULL) {
    goto done;
  }

  if (strcmp("constants_nGatesCellMin", name) == 0) {
    if (PyFloat_Check(val)) {
      self->v2b->constants.nGatesCellMin = (int)PyFloat_AsDouble(val);
    } else if (PyLong_Check(val)) {
        self->v2b->constants.nGatesCellMin = (int)PyLong_AsDouble(val);
    } else if (PyInt_Check(val)) {
        self->v2b->constants.nGatesCellMin = (int)PyInt_AsLong(val);
    } else {
      raiseException_gotoTag(done, PyExc_ValueError, "constants_nGatesCellMin must be number")
    }
  } else if (strcmp("constants_cellDbzMin", name) == 0) {
    if (PyFloat_Check(val)) {
      self->v2b->constants.cellDbzMin = PyFloat_AsDouble(val);
    } else if (PyLong_Check(val)) {
        self->v2b->constants.cellDbzMin = PyLong_AsDouble(val);
    } else if (PyInt_Check(val)) {
        self->v2b->constants.cellDbzMin = (double)PyInt_AsLong(val);
    } else {
      raiseException_gotoTag(done, PyExc_ValueError, "constants_cellDbzMin must be number")
    }
  } else {
    raiseException_gotoTag(done, PyExc_AttributeError, name);
  }

  result = 0;
done:
  return result;
}

/*@} End of Fmi Image */

/// --------------------------------------------------------------------
/// Type definitions
/// --------------------------------------------------------------------
/*@{ Type definitions */
PyTypeObject PyVol2Bird_Type =
{
  PyObject_HEAD_INIT(NULL)0, /*ob_size*/
  "Vol2BirdCore", /*tp_name*/
  sizeof(PyVol2Bird), /*tp_size*/
  0, /*tp_itemsize*/
  /* methods */
  (destructor)_vol2bird_dealloc, /*tp_dealloc*/
  0, /*tp_print*/
  (getattrfunc)_pyvol2bird_getattr, /*tp_getattr*/
  (setattrfunc)_pyvol2bird_setattr, /*tp_setattr*/
  0, /*tp_compare*/
  0, /*tp_repr*/
  0, /*tp_as_number */
  0,
  0, /*tp_as_mapping */
  0 /*tp_hash*/
};
/*@} End of Type definitions */

/*@{ Functions */

/*@} End of Functions */

/*@{ Module setup */
static PyMethodDef functions[] = {
  {"new", (PyCFunction)_pyvol2bird_new, 1},
  {NULL,NULL} /*Sentinel*/
};

PyMODINIT_FUNC
init_vol2bird(void)
{
  PyObject *module=NULL,*dictionary=NULL;
  static void *PyVol2Bird_API[PyVol2Bird_API_pointers];
  PyObject *c_api_object = NULL;
  PyVol2Bird_Type.ob_type = &PyType_Type;

  module = Py_InitModule("_vol2bird", functions);
  if (module == NULL) {
    return;
  }
  PyVol2Bird_API[PyVol2Bird_Type_NUM] = (void*)&PyVol2Bird_Type;
  PyVol2Bird_API[PyVol2Bird_GetNative_NUM] = (void *)PyVol2Bird_GetNative;
  PyVol2Bird_API[PyVol2Bird_New_NUM] = (void*)PyVol2Bird_New;

  c_api_object = PyCObject_FromVoidPtr((void *)PyVol2Bird_API, NULL);

  if (c_api_object != NULL) {
    PyModule_AddObject(module, "_C_API", c_api_object);
  }

  dictionary = PyModule_GetDict(module);
  ErrorObject = PyString_FromString("_vol2bird.error");

  if (ErrorObject == NULL || PyDict_SetItemString(dictionary, "error", ErrorObject) != 0) {
    Py_FatalError("Can't define _vol2bird.error");
  }
  import_pypolarvolume();
  import_pyverticalprofile();
  PYRAVE_DEBUG_INITIALIZE;
}
/*@} End of Module setup */