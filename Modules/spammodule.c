/* just following the tutorial https://docs.python.org/3/extending/extending.html */
#define PY_SSIZE_T_CLEAN
#include "Python.h"

static PyObject *SpamError = NULL;

static PyObject *spam_system(PyObject *self, PyObject *args)
{
    const char *command;
    int system_result;

    if (!PyArg_ParseTuple(args, "s", &command)) {
        return NULL;
    }
    system_result = system(command);
    if (system_result < 0) {
        PyErr_SetString(SpamError, "System Command Failed");
        return NULL;
    }
    return PyLong_FromLong(system_result);
    Py_RETURN_NONE;
}

static int spam_module_exec(PyObject *m)
{
    if (SpamError != NULL) {
        PyErr_SetString(PyExc_ImportError,
                        "cannot initialize spam module more than once");
        return -1;
    }
    SpamError = PyErr_NewException("spam.error", NULL, NULL);
    if (PyModule_AddObjectRef(m, "SpamError", SpamError) < 0) {
        return -1;
    }

    return 0;
}

static PyModuleDef_Slot spam_module_slots[] = {
    {Py_mod_exec, spam_module_exec},
    {0, NULL}
};


static int spam_traverse(PyObject *module, visitproc visit, void *arg) {
    Py_VISIT(SpamError);
    return 0;
}

static int spam_clear(PyObject *module) {
    Py_CLEAR(SpamError);
    return 0;
}

static void spam_free(void *module)
{
    // allow spam_module_exec to omit calling spam_clear on error
    (void)spam_clear((PyObject *)module);
}

static PyMethodDef spam_methods[] = {
    {"system", spam_system, METH_VARARGS, "Execute a shell command."},
    {NULL, NULL, 0, NULL},        /* Sentinel */
};

static struct PyModuleDef spam_module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "spam",
    .m_size = 0,  // non-negative
    .m_slots = spam_module_slots,
    .m_methods = spam_methods,
    .m_traverse = spam_traverse,
    .m_clear = spam_clear,
    .m_free = spam_free,
};


PyMODINIT_FUNC PyInit_spam(void) {
    return PyModuleDef_Init(&spam_module);
}
