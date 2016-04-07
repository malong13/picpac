#include <boost/ref.hpp>
#include <boost/python.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/raw_function.hpp>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/ndarrayobject.h>
#include "picpac.h"
#include "picpac-cv.h"
using namespace boost::python;
using namespace picpac;

namespace {

template <typename T>
T *get_ndarray_data (object &o) {
    PyArrayObject *nd = reinterpret_cast<PyArrayObject *>(o.ptr());
    return reinterpret_cast<T*>(PyArray_DATA(nd));
}

class NumpyBatchImageStream: public BatchImageStream {
public:
    NumpyBatchImageStream (std::string const &path, Config const &c)
        : BatchImageStream(fs::path(path), c) {
        import_array();
    }
    tuple next () {
        vector<npy_intp> images_dims;
        vector<npy_intp> labels_dims;
        next_shape(&images_dims, &labels_dims);
        object images = object(boost::python::handle<>(PyArray_SimpleNew(images_dims.size(), &images_dims[0], NPY_FLOAT)));
        CHECK(images.ptr());
        float *images_buf = get_ndarray_data<float>(images);
        object labels = object(boost::python::handle<>(PyArray_SimpleNew(labels_dims.size(), &labels_dims[0], NPY_FLOAT)));
        CHECK(labels.ptr());
        float *labels_buf = get_ndarray_data<float>(labels);
        unsigned padding;
        next_fill(images_buf, labels_buf, &padding);
        return make_tuple(images, labels, padding);
    }
};

object create_image_stream (tuple args, dict kwargs) {
    object self = args[0];
    CHECK(len(args) > 1);
    string path = extract<string>(args[1]);
    NumpyBatchImageStream::Config config;
    /*
    bool train = extract<bool>(kwargs.get("train", true));
    unsigned K = extract<unsigned>(kwargs.get("K", 1));
    unsigned fold = extract<unsigned>(kwargs.get("fold", 0));
    if (K <= 1) {
        if (!train) {
            config.loop = false;
            config.reshuffle = false;
        }
    }
    else {
        config.kfold(K, fold, train);
    }
    */
#define PICPAC_CONFIG_UPDATE(C, P) \
    C.P = extract<decltype(C.P)>(kwargs.get(#P, C.P))
    PICPAC_CONFIG_UPDATE_ALL(config);
#undef PICPAC_CONFIG_UPDATE
    return self.attr("__init__")(path, config);
};


class Writer: public FileWriter {
    void encode (PyArrayObject *image, string *) {
        
    }
public:
    Writer (string const &path): FileWriter(fs::path(path)) {
    }
    void append (float label, string const &buf) {
        Record record(label, buf);
        FileWriter::append(record);
    }

    void append (string const &buf1, string const &buf2) {
        Record record(0, buf1, buf2);
        FileWriter::append(record);
    }
};

void (Writer::*append1) (float, string const &) = &Writer::append;
void (Writer::*append2) (string const &, string const &) = &Writer::append;

void translate_eos (EoS const &)
{
    // Use the Python 'C' API to set up an exception object
    PyErr_SetNone(PyExc_StopIteration);
}

}

BOOST_PYTHON_MODULE(_picpac)
{
    scope().attr("__doc__") = "PicPoc Python API";
    register_exception_translator<EoS>(&translate_eos);
    class_<NumpyBatchImageStream::Config>("ImageStreamParams", init<>());
    class_<NumpyBatchImageStream, boost::noncopyable>("ImageStream", no_init)
        .def("__init__", raw_function(create_image_stream), "exposed ctor")
        .def(init<string, NumpyBatchImageStream::Config const&>()) // C++ constructor not exposed
        .def("next", &NumpyBatchImageStream::next)
        .def("size", &NumpyBatchImageStream::size)
        .def("reset", &NumpyBatchImageStream::reset)
    ;
    class_<Writer>("Writer", init<string>())
        .def("append", append1)
        .def("append", append2)
    ;
}
