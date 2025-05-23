#include <Python.h>
#include <numpy/arrayobject.h>

#define MAX_ARRAYS 1800

static PyObject *_Error;


typedef npy_float64 (*combiner)(int, int, int, int, npy_float64 temp[MAX_ARRAYS]);


static int
_mask_and_sort(int ninputs, int index, int fill, npy_float64 **inputs,
               npy_uint8 **masks, npy_float64 temp[MAX_ARRAYS])
{
    int i, j, goodpix;
    npy_float64 temp2;

    if (masks) {
        for (i=j=0; i<ninputs; i++) {
            if (masks[i][index] == 0) {
                temp[j++] = inputs[i][index];
            }
        }
        if (j == 0 && fill == 1) {
            for (i=0; i<ninputs; i++) {
                if (inputs[i][index] != 0){
                    temp[j++] = inputs[i][index];
                    break;
                }
            }
        }
    } else {
        for (i=j=0; i<ninputs; i++) {
            temp[j++] = inputs[i][index];
        }
    }
    goodpix = j;
    for(i=0; i<goodpix; i++) {
        for (j=i+1; j<goodpix; j++) {
            if (temp[j] < temp[i]) {
                temp2 = temp[j];
                temp[j] = temp[i];
                temp[i] = temp2;
            }
        }
    }
    return goodpix;
}


static npy_float64
_inner_median(int goodpix, int nlow, int nhigh, int ninputs,
              npy_float64 temp[MAX_ARRAYS])
{
    npy_float64 median;
    int midpoint, medianpix = goodpix-nhigh-nlow;
    if (medianpix <= 0 && goodpix > 0) {
        /*
         * nhigh/nlow discarded all the good pixels
         * reduce them until we have something left
         */
        while (nhigh+nlow >= goodpix) {
            if (nhigh > 0) nhigh = nhigh-1;
            if (nlow > 0) nlow = nlow-1;
        }
        medianpix = goodpix-nhigh-nlow;
    }
    if (medianpix <= 0) {
        if (ninputs > 0) {
            median = temp[ninputs-1];
        } else{
            median = 0;
        }
    } else {
        midpoint = medianpix / 2;
        if (medianpix % 2) /* odd */ {
            median = temp[ midpoint + nlow ];
        } else {
            median = (temp[ midpoint + nlow ] +
                      temp[midpoint + nlow - 1 ]) / 2.0;
        }
    }
    return median;
}


static npy_float64
_inner_old_median(int goodpix, int nlow, int nhigh, int ninputs,
                  npy_float64 temp[MAX_ARRAYS])
{
    npy_float64 median;
    int midpoint, medianpix = goodpix-nhigh-nlow;
    if (medianpix <= 0) {
        median = 0;
    } else {
        midpoint = medianpix / 2;
        if (medianpix % 2) /* odd */ {
            median = temp[ midpoint + nlow ];
        } else {
            median = (temp[ midpoint + nlow ] +
                  temp[ midpoint + nlow - 1 ]) / 2.0;
        }
    }
    return median;
}


static npy_float64
_inner_average(int goodpix, int nlow, int nhigh, int ninputs,
               npy_float64 temp[MAX_ARRAYS])
{
    npy_float64 average;
    int i, averagepix = goodpix - nhigh - nlow;

    if (averagepix <= 0) {
        if (ninputs > 0) {
            average = temp[ninputs-1];
        } else{
            average = 0;
        }
    } else {
        for(i=nlow, average=0; i<averagepix+nlow;  i++) {
            average += temp[i];
        }
        average /= averagepix;
    }
    return average;
}


static npy_float64
_inner_minimum(int goodpix, int nlow, int nhigh, int ninputs,
               npy_float64 temp[MAX_ARRAYS])
{
    int minimumpix = goodpix - nhigh - nlow;
    if (minimumpix <= 0) {
        return 0;
    } else {
        return temp[nlow];
    }
}


static int
_combine(combiner f, int dim, int maxdim, int ninputs, int nlow, int nhigh,
         int fillval, PyArrayObject *inputs[], PyArrayObject *masks[],
         PyArrayObject *output)
{
    int i, j;

    if (dim == maxdim-1) {
        npy_float64 sorted[MAX_ARRAYS];
        npy_float64 *tinputs[MAX_ARRAYS];
        npy_uint8    *tmasks[MAX_ARRAYS];
        npy_float64 *toutput;
        int cols = inputs[0]->dimensions[dim];

        /* Allocate and convert 1 temporary row at a time */
        for(i=0; i<ninputs; i++) {
            tinputs[i] = (npy_float64 *) inputs[i]->data;
        }
        if (masks) {
            for(i=0; i<ninputs; i++) {
                tmasks[i] = (npy_uint8 *) masks[i]->data;
            }
        }
        toutput = (npy_float64 *) output->data;

        for(j=0; j<cols; j++) {
            int goodpix = _mask_and_sort(
                ninputs, j, fillval, tinputs, masks ? tmasks : NULL, sorted);
            if (fillval == 1) fillval = ninputs;
            toutput[j] = f(goodpix, nlow, nhigh, ninputs, sorted);
            for (i=0;i<ninputs;i++)
                sorted[i] = 0;
        }
    } else {
        for (i=0; i<inputs[0]->dimensions[dim]; i++) {
            for(j=0; j<ninputs; j++) {
                inputs[j]->data += inputs[j]->strides[dim]*i;
                if (masks) {
                    masks[j]->data += masks[j]->strides[dim]*i;
                }
            }
            output->data += output->strides[dim]*i;
            _combine(f, dim+1, maxdim, ninputs, nlow, nhigh, fillval,
                     inputs, masks, output);
            for(j=0; j<ninputs; j++) {
                inputs[j]->data -= inputs[j]->strides[dim]*i;
                if (masks) {
                    masks[j]->data -= masks[j]->strides[dim]*i;
                }
            }
            output->data -= output->strides[dim]*i;
        }
    }
    return 0;
}


typedef struct
{
    char *name;
    combiner fptr;
} fmapping;


static fmapping functions[] = {
    {"median", _inner_median},
    {"average", _inner_average},
    {"minimum", _inner_minimum},
    {"imedian", _inner_median},
    {"iaverage", _inner_average},
};


static PyObject *
_Py_combine(PyObject *obj, PyObject *args, PyObject *kw)
{
    PyObject   *arrays, *output;
    int        nlow=0, nhigh=0, narrays;
    PyObject   *badmasks=Py_None;
    char       *keywds[] = { "arrays", "output", "nlow", "nhigh",
                             "badmasks", "kind", NULL };
    char *kind;
    combiner f;
    PyArrayObject *arr[MAX_ARRAYS], *bmk[MAX_ARRAYS], *toutput;
    int i;
    int fillval = 0;
    char fname[] = " ";

    if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|iiOs:combine", keywds,
             &arrays, &output, &nlow, &nhigh, &badmasks, &kind)) {
        return NULL;
    }

    narrays = PySequence_Length(arrays);
    if (narrays < 0) {
        return PyErr_Format(
            PyExc_TypeError, "combine: arrays is not a sequence");
    }
    if (narrays > MAX_ARRAYS) {
        return PyErr_Format(
            PyExc_TypeError, "combine: too many arrays.");
    }

    for(i=0; i<narrays; i++) {
        PyObject *a = PySequence_GetItem(arrays, i);
        if (!a) {
            return NULL;
        }
        arr[i] = (PyArrayObject *) PyArray_FROM_OTF(a, NPY_FLOAT64,
                                                    NPY_ARRAY_INOUT_ARRAY2);
        if (!arr[i]) {
            return NULL;
        }
        Py_DECREF(a);
        if (badmasks != Py_None) {
            a =  PySequence_GetItem(badmasks, i);
            if (!a) {
                return NULL;
            }
            bmk[i] = (PyArrayObject *) PyArray_FROM_OTF(a, NPY_UINT8,
                                                        NPY_ARRAY_IN_ARRAY);
            if (!bmk[i]) {
                return NULL;
            }
            Py_DECREF(a);
        }
    }

    toutput = (PyArrayObject *) PyArray_FROM_OTF(output, NPY_FLOAT64,
                                                 NPY_ARRAY_INOUT_ARRAY2);
    if (!toutput) {
        return NULL;
    }

    for (i=0,f=0; i<(sizeof(functions)/sizeof(functions[0])); i++)
        if  (!strcmp(kind, functions[i].name)) {
            f = functions[i].fptr;
            strncpy(fname,functions[i].name,1);
            if (!strcmp(fname,"i")) {
                fillval = 1;
            }
            break;
        }
    if (!f)    return PyErr_Format(
        PyExc_ValueError, "Invalid comination function.");

    if (_combine(f, 0, arr[0]->nd, narrays, nlow, nhigh, fillval, arr,
                 (badmasks != Py_None ? bmk : NULL), toutput) < 0) {
        return NULL;
    }

    for(i=0; i<narrays; i++) {
        PyArray_ResolveWritebackIfCopy(arr[i]);
        Py_DECREF(arr[i]);
        if (badmasks != Py_None) {
            PyArray_ResolveWritebackIfCopy(bmk[i]);
            Py_DECREF(bmk[i]);
        }
    }
    PyArray_ResolveWritebackIfCopy(toutput);
    Py_DECREF(toutput);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef _combineMethods[] = {
    {"combine", (PyCFunction) _Py_combine, METH_VARARGS | METH_KEYWORDS},
    {NULL, NULL} /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  "_combine",          /* m_name */
  "Combine module",    /* m_doc */
  -1,                  /* m_size */
  _combineMethods,     /* m_methods */
  NULL,                /* m_reload */
  NULL,                /* m_traverse */
  NULL,                /* m_clear */
  NULL,                /* m_free */
};
#endif

PyMODINIT_FUNC
#if PY_MAJOR_VERSION >= 3
PyInit__combine(void)
#else
init_combine(void)
#endif
{
    PyObject *m, *d;
#if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&moduledef);
#else
    m = Py_InitModule("_combine", _combineMethods);
#endif
    d = PyModule_GetDict(m);
    _Error = PyErr_NewException("_combine.error", NULL, NULL);
    PyDict_SetItemString(d, "error", _Error);
    import_array();
#if PY_MAJOR_VERSION >= 3
	return m;
#else
	return;
#endif
}
