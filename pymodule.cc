#include <python2.7/Python.h>
#include <sstream>
#include <iostream>
#include "search.hh"

static PyObject * py_init(PyObject *self, PyObject *args)
{
	PyObject *list;
	if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &list)) {
		PyErr_SetString(PyExc_RuntimeError, "error parsing list");
		return NULL;
	}
	size_t len(PyList_Size(list));
	std::vector<std::string> filenames;
	for (size_t i(0); i < len; ++i) {
		PyObject *sobj(PyList_GetItem(list, i));
		if (!sobj) {
			continue;
		}
		char *s(PyString_AsString(sobj));
		if (!s) {
			continue;
		}
		filenames.push_back(s);
	}
	size_t count(init_indices(filenames));
	return Py_BuildValue("i", count);
}

static PyObject * py_search(PyObject *self, PyObject *args)
{
	char *term;
	if (!PyArg_ParseTuple(args, "s", &term)) {
		PyErr_SetString(PyExc_RuntimeError, "error parsing string");
		return NULL;
	}
	search_results r(search_indices(term));
	std::ostringstream oss;
	oss << "{\"hits\":" << r.total << ", ";
	oss << "\"top\": [\n";
	typedef std::vector<search_result>::const_iterator srcit;
	for (srcit it(r.top.begin()); it != r.top.end(); ++it) {
		oss << (it == r.top.begin() ? "" : ",\n");
		oss << "\t{\"article\": \"" << it->article << "\", ";
		oss << "\"weight\": " << it->weight << "}";
	}
	oss << "\n]}";
	return Py_BuildValue("s", oss.str().c_str());
}

static PyMethodDef module_methods[] = {
	{ "init",   py_init,   METH_VARARGS },
	{ "search", py_search, METH_VARARGS },
	{ NULL, NULL }
};

extern "C" {
	void initindisk()
	{
		Py_InitModule("indisk", module_methods);
	}
}

