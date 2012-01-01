#include <Python.h>
#include <string>

class dummy
{
public:
	dummy() : i(123) { }
	int i;
};

dummy D;

static PyObject * py_search(PyObject *self, PyObject *args)
{
	std::string s("Hello from C++");
	return Py_BuildValue("si", s.c_str(), D.i);
}

static PyMethodDef readermodule_methods[] = {
	{ "search", py_search, METH_VARARGS },
	{ NULL, NULL }
};

extern "C" {
	void initreadermodule()
	{
		(void) Py_InitModule("readermodule", readermodule_methods);
	}
}

