#ifdef __CPLUSPLUS__
extern "C" {
#endif

#ifndef __GNUC__
#pragma warning(disable: 4275)
#pragma warning(disable: 4101)

#endif
#include "Python.h"
#include <complex>
#include <math.h>
#include <string>
#include <iostream>
#include <exception>
#include <stdio.h>
#include "numpy/arrayobject.h"
#include "vtkCellArray.h"
#include "vtkPolyDataReader.h"
#include "vtkXMLPolyDataReader.h"
#include "vtkPolyData.h"
#include "vtkPointData.h"
#include "vtkSmartPointer.h"


  static NPY_TYPES getNumpyDataTypeFromVTKDataType( int dataType, bool& success )
  {
    NPY_TYPES t = NPY_FLOAT64;

    if ( dataType == VTK_ID_TYPE )
    {
      #if    VTK_SIZEOF_ID_TYPE==1
        dataType = VTK_TYPE_UINT8;
      #elif  VTK_SIZEOF_ID_TYPE==2
        dataType = VTK_TYPE_UINT16;
      #elif  VTK_SIZEOF_ID_TYPE==4
        dataType = VTK_TYPE_UINT32;
      #elif  VTK_SIZEOF_ID_TYPE==8
        dataType = VTK_TYPE_INT64; //In this code VTK_LONG gets mapped tp NPY_INT64
      #else
        dataType = VTK_INT;
      #endif
     }

    success = true;
     
    switch ( dataType )
      {
      case VTK_TYPE_INT8          : t = NPY_INT8; break;
      case VTK_TYPE_UINT8         : t = NPY_UINT8; break;
      case VTK_TYPE_INT16         : t = NPY_INT16; break;
      case VTK_TYPE_UINT16        : t = NPY_UINT16; break;
      case VTK_TYPE_INT32         : t = NPY_INT32; break;
      case VTK_TYPE_UINT32        : t = NPY_UINT32; break;
      case VTK_TYPE_INT64         : t = NPY_INT64; break;
      case VTK_TYPE_UINT64        : t = NPY_UINT64; break;
      case VTK_TYPE_FLOAT32       : t = NPY_FLOAT32; break;
      case VTK_TYPE_FLOAT64       : t = NPY_FLOAT64; break;
      default:
        success = false;
      }

      return t;
  }

PyArrayObject* vtkArrayToNpyArray( vtkSmartPointer<vtkDataArray> da, bool& success )
{

  npy_intp dim_ptrs[2];
  dim_ptrs[0] = static_cast<npy_intp> (da->GetNumberOfTuples( ));
  dim_ptrs[1] = static_cast<npy_intp> (da->GetNumberOfComponents());

  PyArrayObject* array = NULL;
  array = (PyArrayObject*) PyArray_SimpleNew ( 2, dim_ptrs, getNumpyDataTypeFromVTKDataType( da->GetDataType(),success ) );

  if ((success) && (array==NULL))
  {
    success = false;
  }

  if (success)
  {
    da->ExportToVoidPointer( array->data );
    return array;
  } else {
    PyErr_Format ( PyExc_TypeError, "Could not find unknown datatype" );
    return NULL;
  }

}

static PyObject* c_polyDataReader(PyObject*self, PyObject* args, PyObject* kywds)
{
    PyObject* return_val;
    int exception_occured = 0;
    PyObject *py_local_dict = NULL;
    static char *kwlist[] = {"fname","local_dict", NULL};
    static char *parameters ="O|O:c_polyDataReader";

    PyObject *py_fname;
    py_fname = NULL;
   
    if(!PyArg_ParseTupleAndKeywords(args,kywds,parameters,kwlist,&py_fname, &py_local_dict))
       return NULL;
    try                              
    {                                
        py_fname = py_fname;
        std::string fname(PyString_AsString(py_fname));
        /*<function call here>*/     
        
        vtkSmartPointer<vtkXMLPolyDataReader> xml_pdr = vtkXMLPolyDataReader::New();
        vtkSmartPointer<vtkPolyDataReader> pdr = vtkPolyDataReader::New();
        vtkSmartPointer<vtkPolyData> polyData;
        bool success;
        pdr->SetFileName(fname.c_str());
        xml_pdr->SetFileName(fname.c_str());

        if (not pdr->IsFileValid(fname.c_str())) {
          pdr->Update();
          polyData = pdr->GetOutput();
        } else if (not xml_pdr->CanReadFile(fname.c_str())) {
          xml_pdr->Update();
          polyData = xml_pdr->GetOutput();
        } else {
          throw 0;
        }


          vtkSmartPointer<vtkIdTypeArray> ia;
          vtkSmartPointer<vtkDataArray> da;
          vtkSmartPointer<vtkPointData> pd;
        
          ia = polyData->GetLines()->GetData();
          da = polyData->GetPoints()->GetData();
          pd = polyData->GetPointData();
           
          PyArrayObject* array_lines = vtkArrayToNpyArray( ia, success );
          if (!success)
            {
            pdr->Delete();
            xml_pdr->Delete();
            return_val = NULL;
            }
        
          PyArrayObject* array = vtkArrayToNpyArray( da, success );
          if (!success)
            {
            pdr->Delete();
            xml_pdr->Delete();
            return_val = NULL;
            }
        
          PyObject* result = Py_BuildValue("{s:N,s:N,s:l}","lines" ,(PyObject*) array_lines,"points", (PyObject*) array,"numberOfLines",(long int)(pdr->GetOutput()->GetNumberOfLines()));
        
          PyObject* pointData = PyDict_New();
        
          if (pd->GetScalars()!=NULL)
          {
             PyDict_SetItemString(pointData,"ActiveScalars",PyString_FromString(pd->GetScalars()->GetName()));
          }

          if (pd->GetVectors()!=NULL)
          {
             PyDict_SetItemString(pointData,"ActiveVectors",PyString_FromString(pd->GetVectors()->GetName()));
          }

          if (pd->GetTensors()!=NULL)
          {
             PyDict_SetItemString(pointData,"ActiveTensors",PyString_FromString(pd->GetTensors()->GetName()));
          }
        
          for (int i=0; i<pd->GetNumberOfArrays(); i++)
          {
            PyArrayObject* array = vtkArrayToNpyArray( pd->GetArray(i), success );
            if (!success)
                {
                pdr->Delete();
                xml_pdr->Delete();
                Py_DECREF(array_lines);
                Py_DECREF(array);
                Py_DECREF(pointData);
                PyErr_Format ( PyExc_TypeError, "Could not find unknown datatatype" );
                return NULL;
                }
      
             PyDict_SetItemString(pointData,pd->GetArrayName(i),(PyObject*) array);
             Py_DECREF(array);
          }
        
          PyDict_SetItemString(result,"pointData", pointData);
        
          pdr->Delete();
          xml_pdr->Delete();
          return_val = result;
    
    }                                
    catch(...)                       
    {                                
        return_val =  NULL;
        exception_occured = 1;       
    }                                
    if(!(PyObject*)return_val && !exception_occured)
    {
                                  
        return_val = Py_None;            
    }
//    Py_XDECREF(return_val);                                  
    return return_val;
}

static PyMethodDef compiled_methods[] = 
{
    {"c_polyDataReader",(PyCFunction)c_polyDataReader , METH_VARARGS|METH_KEYWORDS},
    {NULL,      NULL}        /* Sentinel */
};

PyMODINIT_FUNC initreadVtkPolyData_ext(void)
{
    
    Py_Initialize();
    import_array();
    PyImport_ImportModule("numpy");
    (void) Py_InitModule("readVtkPolyData_ext", compiled_methods);
}

#ifdef __CPLUSCPLUS__
}
#endif
