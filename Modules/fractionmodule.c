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

#define TO_STRING_METHOD_REPR (0)
#define TO_STRING_METHOD_STR (1)

#define FracObject_Check(tested_object, frac_instance) (Py_IS_TYPE(tested_object, Py_TYPE(frac_instance)))

// #define PRINTF(FORMAT, ...) printf(FORMAT "\n", ##__VA_ARGS__)
#define PRINTF(FORMAT, ...)

#define PY_SSIZE_T_CLEAN
// mkdir /tmp/bababa111;touch /tmp/bababa111/pyconfig-x86_64.h
#include "Python.h"
#include "longobject.h"

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

static PyObject *FracObject_to_string(PyObject *inp, char method) {
    FracObject *frac = (FracObject *)inp;
    _PyUnicodeWriter writer;
    PyObject *s = NULL;

    _PyUnicodeWriter_Init(&writer);
    writer.overallocate = 1;

    if (TO_STRING_METHOD_REPR == method) {
        if (_PyUnicodeWriter_WriteASCIIString(&writer, "Frac(", 5) < 0) {
            goto Error;
        }
    }

    s = PyObject_Repr(frac->numerator);
    ON_NULL_GOTO_ERROR(s);
    if (_PyUnicodeWriter_WriteStr(&writer, s) < 0) {
        Py_DECREF(s);
        goto Error;
    }
    Py_DECREF(s);
    s = NULL;

    if (TO_STRING_METHOD_REPR == method) {
        if (_PyUnicodeWriter_WriteASCIIString(&writer, ", ", 2) < 0) {
            goto Error;
        }
    }
    else if (TO_STRING_METHOD_STR == method) {
        if (_PyUnicodeWriter_WriteChar(&writer, '/') < 0) {
            goto Error;
        }
    }

    s = PyObject_Repr(frac->denominator);
    ON_NULL_GOTO_ERROR(s);
    if (_PyUnicodeWriter_WriteStr(&writer, s) < 0) {
        goto Error;
    }
    Py_DECREF(s);
    s = NULL;

    if (TO_STRING_METHOD_REPR == method) {
        if (_PyUnicodeWriter_WriteChar(&writer, ')') < 0) {
            goto Error;
        }
    }

    return _PyUnicodeWriter_Finish(&writer);
Error:
    Py_XDECREF(s);
    _PyUnicodeWriter_Dealloc(&writer);
    return NULL;
}

static PyObject *FracObject_repr(PyObject *inp) {
    return FracObject_to_string(inp, TO_STRING_METHOD_REPR);
}
static PyObject *FracObject_str(PyObject *inp) {
    return FracObject_to_string(inp, TO_STRING_METHOD_STR);
}

/* Mathematical Operations */
void binop_type_error(PyObject *v, PyObject *w, const char *op_name)
{
    PyErr_Format(PyExc_TypeError,
                 "unsupported operand type(s) for %.100s: "
                 "'%.100s' and '%.100s'",
                 op_name,
                 Py_TYPE(v)->tp_name,
                 Py_TYPE(w)->tp_name);
}

/*** Frac-Frac operations ***/
static FracObject *FracObject_frac_frac_add(FracObject *self, FracObject *other) {
    FracObject *result = NULL;
    PyObject *n1 = NULL;
    PyObject *n2 = NULL;
    PyObject *numerator = NULL;
    PyObject *denominator = NULL;

    n1 = PyNumber_Multiply(self->numerator, other->denominator);
    ON_NULL_GOTO_ERROR(n1);
    n2 = PyNumber_Multiply(other->numerator, self->denominator);
    ON_NULL_GOTO_ERROR(n2);
    denominator = PyNumber_Multiply(self->denominator, other->denominator);
    ON_NULL_GOTO_ERROR(denominator);
    numerator = PyNumber_Add(n1, n2);
    ON_NULL_GOTO_ERROR(numerator);
    result = newFracObject(Py_TYPE(self), numerator, denominator);
    ON_NULL_GOTO_ERROR(result);

    return result;
Error:
    Py_XDECREF(n1);
    Py_XDECREF(n2);
    Py_XDECREF(numerator);
    Py_XDECREF(denominator);
    return NULL;
}

static FracObject *FracObject_frac_frac_multiply(FracObject *self, FracObject *other) {
    FracObject *result = NULL;
    PyObject *numerator = NULL;
    PyObject *denominator = NULL;

    numerator = PyNumber_Multiply(self->numerator, other->numerator);
    ON_NULL_GOTO_ERROR(numerator);
    denominator = PyNumber_Multiply(self->denominator, other->denominator);
    ON_NULL_GOTO_ERROR(denominator);
    result = newFracObject(Py_TYPE(self), numerator, denominator);
    ON_NULL_GOTO_ERROR(result);

    return result;
Error:
    Py_XDECREF(numerator);
    Py_XDECREF(denominator);
    return NULL;
}

/*** Frac-Long operations ***/
static FracObject *FracObject_frac_long_add(FracObject *self, PyObject *other) {
    FracObject *result = NULL;
    PyObject *numerator = NULL;
    PyObject *denominator = NULL;
    PyObject *n2 = NULL;
    // TODO: TODO

    // numerator = PyNumber_Multiply(self->numerator, other);
    // ON_NULL_GOTO_ERROR(numerator);
    // denominator = Py_NewRef(self->denominator);
    // ON_NULL_GOTO_ERROR(denominator);
    // result = newFracObject(Py_TYPE(self), numerator, denominator);
    // ON_NULL_GOTO_ERROR(result);

    return result;
Error:
    Py_XDECREF(numerator);
    Py_XDECREF(denominator);
    return NULL;
}

static FracObject *FracObject_frac_long_multiply(FracObject *self, PyObject *other) {
    FracObject *result = NULL;
    PyObject *numerator = NULL;
    PyObject *denominator = NULL;

    numerator = PyNumber_Multiply(self->numerator, other);
    ON_NULL_GOTO_ERROR(numerator);
    denominator = Py_NewRef(self->denominator);
    ON_NULL_GOTO_ERROR(denominator);
    result = newFracObject(Py_TYPE(self), numerator, denominator);
    ON_NULL_GOTO_ERROR(result);

    return result;
Error:
    Py_XDECREF(numerator);
    Py_XDECREF(denominator);
    return NULL;
}

/*** Slot Math Functions ***/
static FracObject *FracObject_add(FracObject *self, PyObject *other) {
    if (FracObject_Check(other, self)) {
        return FracObject_frac_frac_add(self, (FracObject *)other);
    }
    else if (PyLong_Check(other)) {
        return FracObject_frac_long_add(self, other);
    }
    else {
        binop_type_error((PyObject *)self, other, "+");
        return NULL;
    }
}

static FracObject *FracObject_multiply(FracObject *self, PyObject *other) {
    if (FracObject_Check(other, self)) {
        return FracObject_frac_frac_multiply(self, (FracObject *)other);
    }
    else if (PyLong_Check(other)) {
        return FracObject_frac_long_multiply(self, other);
    }
    else {
        binop_type_error((PyObject *)self, other, "*");
        return NULL;
    }
}

PyDoc_STRVAR(Frac_doc,
             "A class that stores a number as a fraction of two integers A/B");

static PyType_Slot Frac_Type_slots[] = {
    {Py_tp_doc, (char *)Frac_doc},
    {Py_tp_init, FracObject__init__},
    {Py_tp_traverse, FracObject_traverse},
    {Py_tp_clear, FracObject_clear},
    {Py_tp_finalize, FracObject_finalize},
    {Py_tp_dealloc, FracObject_dealloc},
    {Py_tp_repr, FracObject_repr},
    {Py_tp_str, FracObject_str},

    /* Mathematical Methods */
    {Py_nb_add, FracObject_add},
    // {Py_nb_subtract, },
    {Py_nb_multiply, FracObject_multiply},
    // {Py_nb_divmod, },

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

// TODO: method table
static PyMethodDef frac_module_methods[] = {
    {"new", frac_new, METH_VARARGS, frac_new_doc},
    {NULL,              NULL}           /* sentinel */
};

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
    frac_module_methods,
    frac_slots,
    NULL, // TODO: traverse?
    NULL, // TODO: clear
    NULL, // TODO: free
};


PyMODINIT_FUNC PyInit_frac(void) {
    return PyModuleDef_Init(&frac_module);
}
