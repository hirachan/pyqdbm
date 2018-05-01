/* Depot module using dictionary interface */
/* Author: Yoshitaka Hirano */

#include "Python.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "depot.h"

typedef struct {
    char *dptr;
    int   dsize;
} datum;

typedef struct {
    PyObject_HEAD
    DEPOT *depot;
} DepotObject;

static PyTypeObject DepotType;

#define is_depotobject(v) ((v)->ob_type == &DepotType)
#define check_depotobject_open(v) if ((v)->depot == NULL) \
               { PyErr_SetString(DepotError, "DEPOT object has already been closed"); \
                 return NULL; }

static PyObject *DepotError;

// ---- Constructor
static PyObject *depot_new(char *file, int flags, int size)
{
    DepotObject *dp;

    dp = PyObject_New(DepotObject, &DepotType);
    if (dp == NULL)
        return NULL;
    if (!(dp->depot = dpopen(file, flags, size))) {
        PyErr_SetString(DepotError, dperrmsg(dpecode));
        Py_DECREF(dp);
        return NULL;
    }
    return (PyObject *)dp;
}

// ---- Basic Functions
static void _depot_close(DepotObject* self)
{
    if (self->depot) {
        dpclose(self->depot);
        self->depot = NULL;
    }
}

static void depot_dealloc(DepotObject* self)
{
    _depot_close(self);
    PyObject_Del(self);
}

static int depot_length(DepotObject *dp)
{
    if (dp->depot == NULL) {
        PyErr_SetString(DepotError, "DEPOT object has already been closed");
        return -1;
    }
    return dprnum(dp->depot);
}

static PyObject *depot_subscript(DepotObject *dp, register PyObject *key)
{
    datum drec, krec;
    int tmp_size;
    PyObject *ret;

    if (!PyArg_Parse(key, "s#", &krec.dptr, &tmp_size)) {
        PyErr_SetString(PyExc_TypeError,
                        "depot mappings have string indices only");
        return NULL;
    }

    krec.dsize = tmp_size;
    check_depotobject_open(dp);
    drec.dptr = dpget(dp->depot, krec.dptr, krec.dsize, 0, -1, &tmp_size);
    drec.dsize = tmp_size;

    if (!drec.dptr) {
        if (dpecode == DP_ENOITEM) {
            PyErr_SetString(PyExc_KeyError, PyUnicode_AsUTF8(key));
        } else {
            PyErr_SetString(DepotError, dperrmsg(dpecode));
        }
        return NULL;
    }

    ret = PyUnicode_FromStringAndSize(drec.dptr, drec.dsize);
    free(drec.dptr);
    return ret;
}

static int depot_ass_sub(DepotObject *dp, PyObject *v, PyObject *w)
{
    datum krec, drec;
    int tmp_size;

    if (!PyArg_Parse(v, "s#", &krec.dptr, &tmp_size)) {
        PyErr_SetString(PyExc_TypeError,
                        "depot mappings have string indices only");
        return -1;
    }

    krec.dsize = tmp_size;
    if (dp->depot == NULL) {
        PyErr_SetString(DepotError, "DEPOT object has already been closed");
        return -1;
    }

    if (w == NULL) {
        if (dpout(dp->depot, krec.dptr, krec.dsize) == 0) {
            if (dpecode == DP_ENOITEM) {
                PyErr_SetString(PyExc_KeyError, PyUnicode_AsUTF8(v));
            } else {
                PyErr_SetString(DepotError, dperrmsg(dpecode));
            }
            return -1;
        }
    } else {
        if (!PyArg_Parse(w, "s#", &drec.dptr, &tmp_size)) {
            PyErr_SetString(PyExc_TypeError,
                            "depot mappings have string elements only");
            return -1;
        }
        drec.dsize = tmp_size;
        if (dpput(dp->depot, krec.dptr, krec.dsize, drec.dptr, drec.dsize, DP_DOVER) == 0) {
            PyErr_SetString(DepotError, dperrmsg(dpecode));
            return -1;
        }
    }
    return 0;
}

static PyMappingMethods depot_as_mapping = {
    (lenfunc)depot_length,          /*mp_length*/
    (binaryfunc)depot_subscript,    /*mp_subscript*/
    (objobjargproc)depot_ass_sub,   /*mp_ass_subscript*/
};

// ---- methods
static PyObject *depot_close(register DepotObject *dp, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":close")) {
        return NULL;
    }

    _depot_close(dp);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *depot_keys(register DepotObject *dp, PyObject *args)
{
    register PyObject *v, *item;
    datum key;
    int err, tmp_size;

    if (!PyArg_ParseTuple(args, ":keys")) {
        return NULL;
    }

    check_depotobject_open(dp);

    v = PyList_New(0);
    if (v == NULL) {
        return NULL;
    }

    /* Init Iterator */
    if (!dpiterinit(dp->depot)) {
        PyErr_SetString(DepotError, dperrmsg(dpecode));
        return NULL;
    }

    /* Scan Iterator */
    while((key.dptr = dpiternext(dp->depot, &tmp_size)) != NULL) {
        key.dsize = tmp_size;
        item = PyUnicode_FromStringAndSize(key.dptr, key.dsize);
        free(key.dptr);
        if (item == NULL) {
            Py_DECREF(v);
            return NULL;
        }
        err = PyList_Append(v, item);
        Py_DECREF(item);
        if (err != 0) {
            Py_DECREF(v);
            return NULL;
        }
    }
    return v;
}

static PyObject *depot_has_key(register DepotObject *dp, PyObject *args)
{
    datum key;
    int val;
    int tmp_size;

    if (!PyArg_ParseTuple(args, "s#:has_key", &key.dptr, &tmp_size)) {
        return NULL;
    }
    key.dsize = tmp_size;
    check_depotobject_open(dp);
    val = dpvsiz(dp->depot, key.dptr, key.dsize);
    if (val == -1) {
        if (dpecode == DP_ENOITEM) {
            Py_INCREF(Py_False);
            return Py_False;
        } else {
            PyErr_SetString(DepotError, dperrmsg(dpecode));
            return NULL;
        }
    } else {
        Py_INCREF(Py_True);
        return Py_True;
    }
}

static int depot_contains(PyObject *self, PyObject *arg)
{
    datum key;
    int val;
    Py_ssize_t tmp_size;

    DepotObject *dp = (DepotObject *)self;

    if (dp->depot == NULL) {
        PyErr_SetString(DepotError, "DEPOT object has already been closed");
        return -1;
    }
    if (PyUnicode_Check(arg)) {
        key.dptr = PyUnicode_AsUTF8AndSize(arg, &tmp_size);
        key.dsize = tmp_size;
        if (key.dptr == NULL) {
            return -1;
        }
    } else {
        return -1;
    }

    val = dpvsiz(dp->depot, key.dptr, key.dsize);
    if (val == -1) {
        return 0;
    } else {
        return 1;
    }
}

static PyObject *depot_get(register DepotObject *dp, PyObject *args)
{
    datum key, val;
    PyObject *defvalue = Py_None, *ret;
    int tmp_size;

    if (!PyArg_ParseTuple(args, "s#|O:get",
                          &key.dptr, &tmp_size, &defvalue)) {
        return NULL;
    }
    key.dsize = tmp_size;
    check_depotobject_open(dp);
    val.dptr = dpget(dp->depot, key.dptr, key.dsize, 0, -1, &tmp_size);
    val.dsize = tmp_size;
    if (val.dptr != NULL) {
        ret = PyUnicode_FromStringAndSize(val.dptr, val.dsize);
        free(val.dptr);
    } else {
        Py_INCREF(defvalue);
        ret = defvalue;
    }

    return ret;
}

static PyObject *depot_setdefault(register DepotObject *dp, PyObject *args)
{
    datum key, val;
    PyObject *defvalue = NULL;
    int tmp_size;

    if (!PyArg_ParseTuple(args, "s#|S:setdefault",
                          &key.dptr, &tmp_size, &defvalue)) {
        return NULL;
    }
    key.dsize = tmp_size;
    check_depotobject_open(dp);
    val.dptr = dpget(dp->depot, key.dptr, key.dsize, 0, -1, &tmp_size);
    val.dsize = tmp_size;
    if (val.dptr != NULL) {
        PyObject *ret;
        ret = PyUnicode_FromStringAndSize(val.dptr, val.dsize);
        free(val.dptr);
        return ret;
    }

    if (defvalue == NULL) {
        defvalue = PyUnicode_FromStringAndSize(NULL, 0);
        if (defvalue == NULL)
            return NULL;
    } else {
        Py_INCREF(defvalue);
    }

    val.dptr = PyUnicode_AsUTF8(defvalue);
    val.dsize = PyUnicode_GET_SIZE(defvalue);
    if (!dpput(dp->depot, key.dptr, key.dsize, val.dptr, val.dsize, DP_DOVER)) {
        PyErr_SetString(DepotError, dperrmsg(dpecode));
        return NULL;
    }

    return defvalue;
}

extern PyTypeObject PyDepotIterKey_Type;   /* Forward */
extern PyTypeObject PyDepotIterItem_Type;  /* Forward */
extern PyTypeObject PyDepotIterValue_Type; /* Forward */
static PyObject *depotiter_new(DepotObject *, PyTypeObject *);  /* Forward */

static PyObject *depot_iterkeys(DepotObject *dp)
{
    return depotiter_new(dp, &PyDepotIterKey_Type);
}

static PyObject *depot_iteritems(DepotObject *dp)
{
    return depotiter_new(dp, &PyDepotIterItem_Type);
}

static PyObject *depot_itervalues(DepotObject *dp)
{
    return depotiter_new(dp, &PyDepotIterValue_Type);
}

static PyObject *depot__enter__(PyObject *self, PyObject *args)
{
    Py_INCREF(self);
    return self;
}

static PyObject *depot__exit__(PyObject *self, PyObject *args)
{
    _Py_IDENTIFIER(close);
    return _PyObject_CallMethodId(self, &PyId_close, NULL);
}


static PyMethodDef depot_methods[] = {
    {"close", (PyCFunction)depot_close, METH_VARARGS,
     "close()\nClose the database."},
    {"listkeys", (PyCFunction)depot_keys, METH_VARARGS,
     "listkeys() -> list\nReturn a list of all keys in the database."},
    {"has_key", (PyCFunction)depot_has_key, METH_VARARGS,
     "has_key(key} -> boolean\nReturn true if key is in the database."},
    {"get", (PyCFunction)depot_get, METH_VARARGS,
     "get(key[, default]) -> value\n"
     "Return the value for key if present, otherwise default."},
    {"setdefault", (PyCFunction)depot_setdefault, METH_VARARGS,
     "setdefault(key[, default]) -> value\n"
     "Set the value for key into the database.  If key\n"
     "is not in the database, it is inserted with default as the value."},
    {"keys", (PyCFunction)depot_iterkeys, METH_NOARGS,
     "keys() -> an iterator over the keys"},
    {"items", (PyCFunction)depot_iteritems, METH_NOARGS,
     "items() -> an iterator over the (key, value) items"},
    {"values", (PyCFunction)depot_itervalues, METH_NOARGS,
     "values() -> an iterator over the values"},
    {"__enter__", depot__enter__, METH_NOARGS, NULL},
    {"__exit__",  depot__exit__, METH_VARARGS, NULL},
    {NULL, NULL} /* sentinel */
};

/* ----------------------------------------------------------------- */
/* Depot iterator                                                    */
/* ----------------------------------------------------------------- */

typedef struct {
    PyObject_HEAD
    DepotObject *depot;   /* Set to NULL when iterator is exhausted */
    PyObject* di_result;  /* reusable result tuple for iteritems */
} depotiterobject;

static PyObject *depotiter_new(DepotObject *dp, PyTypeObject *itertype)
{
    depotiterobject *di;
    di = PyObject_New(depotiterobject, itertype);
    if (di == NULL) {
        return NULL;
    }
    Py_INCREF(dp);
    di->depot = dp;
    dpiterinit(dp->depot);

    if (itertype == &PyDepotIterItem_Type) {
        di->di_result = PyTuple_Pack(2, Py_None, Py_None);
        if (di->di_result == NULL) {
            Py_DECREF(di);
            return NULL;
        }
    } else {
        di->di_result = NULL;
    }
    return (PyObject *)di;
}

static void depotiter_dealloc(depotiterobject *di)
{
    Py_XDECREF(di->depot);
    Py_XDECREF(di->di_result);
    PyObject_Del(di);
}

static PySequenceMethods depotiter_as_sequence = {
    0, /* sq_concat */
};


static PyObject *depotiter_iternextkey(depotiterobject *di)
{
    datum key;
    DepotObject *d = di->depot;
    int tmp_size;
    PyObject *ret;

    if (d == NULL) {
        return NULL;
    }
    assert(is_depotobject(d));

    if (!(key.dptr = dpiternext(d->depot, &tmp_size))) {
        if (dpecode != DP_ENOITEM) {
            PyErr_SetString(DepotError, dperrmsg(dpecode));
        }
        Py_DECREF(d);
        di->depot = NULL;
        return NULL;
    }
    key.dsize = tmp_size;

    ret = PyUnicode_FromStringAndSize(key.dptr, key.dsize);
    free(key.dptr);

    return ret;
}

static PyObject *depotiter_iternextitem(depotiterobject *di)
{
    datum key, val;
    PyObject *pykey, *pyval, *result = di->di_result;
    int tmp_size;
    DepotObject *d = di->depot;

    if (d == NULL)
        return NULL;
    assert(is_depotobject(d));

    if (!(key.dptr = dpiternext(d->depot, &tmp_size))) {
        if (dpecode != DP_ENOITEM) {
            PyErr_SetString(DepotError, dperrmsg(dpecode));
        }
        goto fail;
    }
    key.dsize = tmp_size;
    pykey = PyUnicode_FromStringAndSize(key.dptr, key.dsize);

    if (!(val.dptr = dpget(d->depot, key.dptr, key.dsize, 0, -1, &tmp_size))) {
        PyErr_SetString(DepotError, dperrmsg(dpecode));
        free(key.dptr);
        Py_DECREF(pykey);
        goto fail;
    }
    val.dsize = tmp_size;
    pyval = PyUnicode_FromStringAndSize(val.dptr, val.dsize);
    free(key.dptr);
    free(val.dptr);

    if (result->ob_refcnt == 1) {
        Py_INCREF(result);
        Py_DECREF(PyTuple_GET_ITEM(result, 0));
        Py_DECREF(PyTuple_GET_ITEM(result, 1));
    } else {
        result = PyTuple_New(2);
        if (result == NULL)
            return NULL;
    }

    PyTuple_SET_ITEM(result, 0, pykey);
    PyTuple_SET_ITEM(result, 1, pyval);
    return result;

fail:
    Py_DECREF(d);
    di->depot = NULL;
    return NULL;
}

static PyObject *depotiter_iternextvalue(depotiterobject *di)
{
    datum key, val;
    PyObject *pyval;
    int tmp_size;
    DepotObject *d = di->depot;

    if (d == NULL) {
        return NULL;
    }
    assert(is_depotobject(d));

    if (!(key.dptr = dpiternext(d->depot, &tmp_size))) {
        if (dpecode != DP_ENOITEM) {
            PyErr_SetString(DepotError, dperrmsg(dpecode));
        }
        goto fail;
    }
    key.dsize = tmp_size;

    if (!(val.dptr = dpget(d->depot, key.dptr, key.dsize, 0, -1, &tmp_size))) {
        PyErr_SetString(DepotError, dperrmsg(dpecode));
        free(key.dptr);
        goto fail;
    }
    val.dsize = tmp_size;
    pyval = PyUnicode_FromStringAndSize(val.dptr, val.dsize);
    free(key.dptr);
    free(val.dptr);

    return pyval;

fail:
    Py_DECREF(d);
    di->depot = NULL;
    return NULL;
}

static PySequenceMethods depot_as_sequence = {
    0,                      /* sq_length */
    0,                      /* sq_concat */
    0,                      /* sq_repeat */
    0,                      /* sq_item */
    0,                      /* sq_slice */
    0,                      /* sq_ass_item */
    0,                      /* sq_ass_slice */
    depot_contains,         /* sq_contains */
    0,                      /* sq_inplace_concat */
    0,                      /* sq_inplace_repeat */
};


static PyTypeObject DepotType = {
    PyVarObject_HEAD_INIT(0, 0)
    "depot.depot",
    sizeof(DepotObject),
    0,
    (destructor)depot_dealloc,          /*tp_dealloc*/
    0,                                  /*tp_print*/
    0,                                  /*tp_getattr*/
    0,                                  /*tp_setattr*/
    0,                                  /*tp_reserved*/
    0,                                  /*tp_repr*/
    0,                                  /*tp_as_number*/
    &depot_as_sequence,                 /*tp_as_sequence*/
    &depot_as_mapping,                  /*tp_as_mapping*/
    0,                                  /*tp_hash*/
    0,                                  /*tp_call*/
    0,                                  /*tp_str*/
    0,                                  /*tp_getattro*/
    0,                                  /*tp_setattro*/
    0,                                  /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,                 /*tp_xxx4*/
    0,                                  /*tp_doc*/
    0,                                  /*tp_traverse*/
    0,                                  /*tp_clear*/
    0,                                  /*tp_richcompare*/
    0,                                  /*tp_weaklistoffset*/
    (getiterfunc)depot_iterkeys,        /*tp_iter*/
    (iternextfunc )depotiter_iternextvalue,  /*tp_iternext*/
    depot_methods,                      /*tp_methods*/
};

PyTypeObject PyDepotIterKey_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "depoty-keyiterator",           /* tp_name */
    sizeof(depotiterobject),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    /* methods */
    (destructor)depotiter_dealloc,  /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    &depotiter_as_sequence,         /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    PyObject_GenericGetAttr,        /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    0,                              /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    (iternextfunc)depotiter_iternextkey, /* tp_iternext */
};


PyTypeObject PyDepotIterItem_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "depot-itemiterator",           /* tp_name */

    sizeof(depotiterobject),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    /* methods */
    (destructor)depotiter_dealloc,  /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    &depotiter_as_sequence,         /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    PyObject_GenericGetAttr,        /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    0,                              /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    (iternextfunc)depotiter_iternextitem, /* tp_iternext */
};

PyTypeObject PyDepotIterValue_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "depot-valueiterator",          /* tp_name */
    sizeof(depotiterobject),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    /* methods */
    (destructor)depotiter_dealloc,  /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    &depotiter_as_sequence,         /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    PyObject_GenericGetAttr,        /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    0,                              /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    (iternextfunc)depotiter_iternextvalue, /* tp_iternext */
};


/* ----------------------------------------------------------------- */
/* depot module                                                      */
/* ----------------------------------------------------------------- */

static PyObject *
depotopen(PyObject *self, PyObject *args)
{
    char *name;
    char *flags = "r";
    int size = -1;
    int iflags;

    if (!PyArg_ParseTuple(args, "s|si:open", &name, &flags, &size))
        return NULL;
    switch (flags[0]) {
        case 'r':
            iflags = DP_OREADER;
            break;
        case 'w':
            iflags = DP_OWRITER;
            break;
        case 'c':
            iflags = DP_OWRITER | DP_OCREAT | DP_OSPARSE;
            break;
        case 'n':
            iflags = DP_OWRITER | DP_OCREAT | DP_OSPARSE | DP_OTRUNC;
            break;
        default:
            PyErr_SetString(DepotError,
                            "arg 2 to open should be 'r', 'w', 'c', or 'n'");
            return NULL;
    }
    return depot_new(name, iflags, size);
}

struct module_state {
    PyObject *error;
};

static PyMethodDef depotmodule_methods[] = {
    { "open", (PyCFunction)depotopen, METH_VARARGS,
      "open(path[, flag[, size]]) -> mapping\n"
      "Return a database object."},
    { 0, 0 },
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "depot",
    NULL,
    sizeof(struct module_state),
    depotmodule_methods,
    NULL,
    NULL, //depotmodule_traverse,
    NULL, //depotmodule_clear,
    NULL
};

PyMODINIT_FUNC
PyInit_depot(void) {
    PyObject *m, *d;

    if (PyType_Ready(&DepotType) < 0)
        return NULL;
    m = PyModule_Create(&moduledef);
    if (m == NULL)
        return NULL;
    d = PyModule_GetDict(m);
    if (DepotError == NULL)
        DepotError = PyErr_NewException("depot.error", NULL, NULL);
    if (DepotError != NULL)
        PyDict_SetItemString(d, "error", DepotError);

    return m;
}

