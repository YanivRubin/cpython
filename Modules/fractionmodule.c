/** Fraction Object Implementation
 * Author: Yaniv Rubin
 * The goal is to create a new type that stores fractions as "A/B",
 * which does not lose accuracy like floats do.
 *
 * I don't do newlines after function return values- that's because I hate that convention.
 */

#define ON_NULL_GOTO(value, label) if(NULL==value) goto label
#define ON_NULL_GOTO_ERROR(value) ON_NULL_GOTO(value, Error)
#define FRACTION_SUCCESS (1)
#define FRACTION_FAILURE (0)

// #define PRINTF(FORMAT, ...) printf(FORMAT "\n", ##__VA_ARGS__)
#define PRINTF(FORMAT, ...)

#define PY_SSIZE_T_CLEAN
#include "Python.h"

// Module state
typedef struct {
    PyTypeObject *Frac_Type;    // Fraction class
} frac_state;

// Instance state
typedef struct {
    PyObject_HEAD
    PyObject               *numerator;
    PyObject               *denominator;
} FracObject;

/* errors aren't announced with a return value but only with set_string */
static int getSignFromLongObject(PyObject *long_object) {
    int overflow = 0;
    long result = PyLong_AsLongAndOverflow(long_object, &overflow);
    if (-1 == result && 0 != overflow) return overflow;
    if (0 == result) return 0;
    return result > 0 ? 1 : -1;
}

static int normalizeNumeratorDenominator(PyObject **numerator, PyObject **denominator) {
    PyObject *normalized_numerator = NULL;
    PyObject *normalized_denominator = NULL;
    PyObject *gcd = NULL;
    PyObject *temp = NULL;
    int denominator_sign = 0;

    gcd = _PyLong_GCD(*numerator, *denominator);
    ON_NULL_GOTO_ERROR(gcd);

    normalized_numerator = PyNumber_FloorDivide(*numerator, gcd);
    ON_NULL_GOTO_ERROR(normalized_numerator);

    normalized_denominator = PyNumber_FloorDivide(*denominator, gcd);
    ON_NULL_GOTO_ERROR(normalized_denominator);

    Py_DECREF(gcd);
    gcd = NULL;

    denominator_sign = getSignFromLongObject(normalized_denominator);
    /* TODO check if error string set! */

    if (0 == denominator_sign) {
        PyErr_SetString(PyExc_ZeroDivisionError, "frac object division by 0");
        goto Error;
    }

    if (-1 == denominator_sign) {
        /* if denominator is negative, negate both numerator & denominator
           (denominator should always be positive) */
        temp = PyNumber_Negative(normalized_numerator);
        ON_NULL_GOTO_ERROR(temp);
        Py_SETREF(normalized_numerator, temp);
        temp = PyNumber_Negative(normalized_denominator);
        ON_NULL_GOTO_ERROR(temp);
        Py_SETREF(normalized_denominator, temp);
    }

    Py_DECREF(*numerator);
    Py_DECREF(*denominator);
    *numerator = normalized_numerator;
    *denominator = normalized_denominator;

    return FRACTION_SUCCESS;
Error:
    Py_XDECREF(gcd);
    Py_XDECREF(normalized_numerator);
    Py_XDECREF(normalized_denominator);
    Py_XDECREF(temp);
    return FRACTION_FAILURE;
}

// creates a new fraction object
static FracObject *newFracObject(PyTypeObject *Frac_Type, PyObject *numerator, PyObject *denominator) {
    FracObject *self;

    if (!normalizeNumeratorDenominator(&numerator, &denominator)) {
        goto Error;
    }

    self = PyObject_GC_New(FracObject, Frac_Type);
    if (self == NULL) {
        return NULL;
    }

    // the object owns the references
    self->numerator = numerator;
    self->denominator = denominator;
    return self;
Error:
    return NULL;
}

static PyObject *frac_new_from_args(PyTypeObject *Frac_Type, PyObject *args) {
    FracObject *rv;
    PyObject *numerator = NULL;
    PyObject *denominator = NULL;
    // default values for fraction: numerator is 0, denominator is 1
    long long a = 0;
    long long b = 1;
    if (!PyArg_ParseTuple(args, "|LL:new", &a, &b)) {
        return NULL;
    }

    numerator = PyLong_FromLong(a);
    ON_NULL_GOTO_ERROR(numerator);
    denominator = PyLong_FromLong(b);
    ON_NULL_GOTO_ERROR(denominator);
    rv = newFracObject(Frac_Type, numerator, denominator);
    ON_NULL_GOTO_ERROR(rv);
    return (PyObject *)rv;

Error:
    Py_XDECREF(numerator);
    Py_XDECREF(denominator);
    return NULL;
}
static PyObject *frac_new(PyObject *m, PyObject *args) {
    frac_state *state = PyModule_GetState(m);
    if (NULL == state) return NULL;

    return frac_new_from_args(state->Frac_Type, args);
}

PyDoc_STRVAR(frac_new_doc,
"Frac(numerator, denominator)\n" // TODO: how do I denote optional arguments again?
// TODO finish this doc
);

/** Frac __init__.
 * Works by creating a new frac object, copying it's data to self, and deleting the new object.
 */
static int FracObject__init__(PyObject *myself, PyObject *args, PyObject *kwargs) {
    FracObject *self = (FracObject *)myself;
    FracObject *new_frac = NULL;

    new_frac = (FracObject *)frac_new_from_args(Py_TYPE(self), args);
    if (NULL == new_frac) {
        return -1;
    }

    self->numerator = new_frac->numerator;
    self->denominator = new_frac->denominator;
    Py_INCREF(self->numerator);
    Py_INCREF(self->denominator);
    Py_DECREF(new_frac);
    return 0;
}

static int FracObject_traverse(FracObject *self, visitproc visit, void *arg) {
    Py_VISIT(Py_TYPE(self)); // Visit the type
    Py_VISIT(self->numerator);
    Py_VISIT(self->denominator);
    return 0;
}

static int FracObject_clear(FracObject *self)
{
    Py_CLEAR(self->numerator);
    Py_CLEAR(self->denominator);
    return 0;
}

static void FracObject_finalize(PyObject *self_obj)
{
    FracObject *self = (FracObject *)self_obj;
    Py_CLEAR(self->numerator);
    Py_CLEAR(self->denominator);
}

static void FracObject_dealloc(PyObject *self)
{
    PyObject_GC_UnTrack(self);
    FracObject_finalize(self);
    PyTypeObject *tp = Py_TYPE(self);
    freefunc free = PyType_GetSlot(tp, Py_tp_free);
    if (NULL != free) free(self);
    Py_DECREF(tp);
}

static PyObject *FracObject_to_string(PyObject *inp) {
    FracObject *frac = (FracObject *)inp;
    _PyUnicodeWriter writer;
    PyObject *s;

    _PyUnicodeWriter_Init(&writer);
    writer.overallocate = 1;

    s = PyObject_Repr(frac->numerator);
    ON_NULL_GOTO_ERROR(s);
    if (_PyUnicodeWriter_WriteStr(&writer, s) < 0) {
        Py_DECREF(s);
        goto Error;
    }
    Py_DECREF(s);

    if (_PyUnicodeWriter_WriteChar(&writer, '/') < 0) {
        goto Error;
    }

    s = PyObject_Repr(frac->denominator);
    ON_NULL_GOTO_ERROR(s);
    if (_PyUnicodeWriter_WriteStr(&writer, s) < 0) {
        Py_DECREF(s);
        goto Error;
    }
    Py_DECREF(s);

    return _PyUnicodeWriter_Finish(&writer);
Error:
    _PyUnicodeWriter_Dealloc(&writer);
    return NULL;
}

// TODO: method table
static PyMethodDef frac_methods[] = {
    {"new", frac_new, METH_VARARGS, frac_new_doc},
    {NULL,              NULL}           /* sentinel */
};

PyDoc_STRVAR(Frac_doc,
             "A class that stores a number as a fraction of two integers A/B");

static PyType_Slot Frac_Type_slots[] = {
    {Py_tp_doc, (char *)Frac_doc},
    {Py_tp_init, FracObject__init__},
    {Py_tp_traverse, FracObject_traverse},
    {Py_tp_clear, FracObject_clear},
    {Py_tp_finalize, FracObject_finalize},
    {Py_tp_dealloc, FracObject_dealloc},
    {Py_tp_repr, FracObject_to_string},

    // {Py_tp_methods, Frac_methods},

    {0, 0},  /* sentinel */
};

static PyType_Spec Frac_Type_spec = {
    .name = "frac.Frac",
    .basicsize = sizeof(FracObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .slots = Frac_Type_slots,
};

static int frac_exec(PyObject *m) {
    frac_state *state = PyModule_GetState(m);

    state->Frac_Type = (PyTypeObject *)PyType_FromModuleAndSpec(m, &Frac_Type_spec, NULL);
    if (state->Frac_Type == NULL) {
        return -1;
    }

    if (PyModule_AddType(m, (PyTypeObject*)state->Frac_Type) < 0) {
        return -1;
    }

    PRINTF("frac_exec done\n");

    return 0;
}

static struct PyModuleDef_Slot frac_slots[] = {
    {Py_mod_exec, frac_exec},
    // TODO: what are these?
    // {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    // {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL},
};

PyDoc_STRVAR(module_doc,
"This module implements fraction support\n"
"Made for fun by Yaniv Rubin");

static struct PyModuleDef frac_module = {
    PyModuleDef_HEAD_INIT,
    "frac",
    module_doc,
    0,
    frac_methods,
    frac_slots,
    NULL, // TODO: traverse?
    NULL, // TODO: clear
    NULL, // TODO: free
};


PyMODINIT_FUNC PyInit_frac(void) {
    return PyModuleDef_Init(&frac_module);
}
