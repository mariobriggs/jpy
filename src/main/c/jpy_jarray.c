#include "jpy_module.h"
#include "jpy_jarray.h"


#define PRINT_FLAG(F) printf("JArray_GetBufferProc: %s = %d\n", #F, (flags & F) != 0);
#define PRINT_MEMB(F, M) printf("JArray_GetBufferProc: %s = " ## F ## "\n", #M, M);

//#define JPy_USE_GET_PRIMITIVE_ARRAY_CRITICAL 1


/*
 * Implements the getbuffer() method of the buffer protocol for JPy_JArray objects.
 * Regarding the format parameter, refer to the Python 'struct' module documentation:
 * http://docs.python.org/2/library/struct.html#module-struct
 */
int JArray_GetBufferProc(JPy_JArray* self, Py_buffer* view, int flags, char javaType, jint itemSize, const char* format)
{
    JNIEnv* jenv;
    jint itemCount;
    jboolean isCopy;
    void* buf;

    JPy_GET_JNI_ENV_OR_RETURN(jenv, -1)

    /*
    printf("JArray_GetBufferProc:\n");
    PRINT_FLAG(PyBUF_ANY_CONTIGUOUS);
    PRINT_FLAG(PyBUF_CONTIG);
    PRINT_FLAG(PyBUF_CONTIG_RO);
    PRINT_FLAG(PyBUF_C_CONTIGUOUS);
    PRINT_FLAG(PyBUF_FORMAT);
    PRINT_FLAG(PyBUF_FULL);
    PRINT_FLAG(PyBUF_FULL_RO);
    PRINT_FLAG(PyBUF_F_CONTIGUOUS);
    PRINT_FLAG(PyBUF_INDIRECT);
    PRINT_FLAG(PyBUF_ND);
    PRINT_FLAG(PyBUF_READ);
    PRINT_FLAG(PyBUF_RECORDS);
    PRINT_FLAG(PyBUF_RECORDS_RO);
    PRINT_FLAG(PyBUF_SIMPLE);
    PRINT_FLAG(PyBUF_STRIDED);
    PRINT_FLAG(PyBUF_STRIDED_RO);
    PRINT_FLAG(PyBUF_STRIDES);
    PRINT_FLAG(PyBUF_WRITE);
    PRINT_FLAG(PyBUF_WRITEABLE);
    */

    itemCount = (*jenv)->GetArrayLength(jenv, self->objectRef);

    // According to Python documentation,
    // buffer allocation shall be done in the 5 following steps;

    // Step 1/5
#ifdef JPy_USE_GET_PRIMITIVE_ARRAY_CRITICAL
    buf = (*jenv)->GetPrimitiveArrayCritical(jenv, self->objectRef, &isCopy);
#else
    if (javaType == 'Z') {
        buf = (*jenv)->GetBooleanArrayElements(jenv, self->objectRef, &isCopy);
    } else if (javaType == 'C') {
        buf = (*jenv)->GetCharArrayElements(jenv, self->objectRef, &isCopy);
    } else if (javaType == 'B') {
        buf = (*jenv)->GetByteArrayElements(jenv, self->objectRef, &isCopy);
    } else if (javaType == 'S') {
        buf = (*jenv)->GetShortArrayElements(jenv, self->objectRef, &isCopy);
    } else if (javaType == 'I') {
        buf = (*jenv)->GetIntArrayElements(jenv, self->objectRef, &isCopy);
    } else if (javaType == 'J') {
        buf = (*jenv)->GetLongArrayElements(jenv, self->objectRef, &isCopy);
    } else if (javaType == 'F') {
        buf = (*jenv)->GetFloatArrayElements(jenv, self->objectRef, &isCopy);
    } else if (javaType == 'D') {
        buf = (*jenv)->GetDoubleArrayElements(jenv, self->objectRef, &isCopy);
    } else {
        PyErr_Format(PyExc_RuntimeError, "internal error: illegal Java array type '%c'", javaType);
        return -1;
    }
#endif
    if (buf == NULL) {
        PyErr_NoMemory();
        return -1;
    }

    JPy_DEBUG_PRINTF("JArray_GetBufferProc: buf=%p, type='%s', format='%s', itemSize=%d, itemCount=%d, isCopy=%d\n", buf, Py_TYPE(self)->tp_name, format, itemSize, itemCount, isCopy);

    // Step 2/5
    view->buf = buf;
    view->len = itemCount * itemSize;
    view->itemsize = itemSize;
    view->readonly = (flags & (PyBUF_WRITE | PyBUF_WRITEABLE)) == 0;
    view->ndim = 1;
    view->shape = PyMem_New(Py_ssize_t, 1);
    *view->shape = itemCount;
    view->strides = PyMem_New(Py_ssize_t, 1);
    *view->strides = itemSize;
    view->suboffsets = NULL;
    if ((flags & PyBUF_FORMAT) != 0) {
        view->format = (char*) format;
    } else {
        view->format = (char*) "B";
    }

    /*
    PRINT_MEMB("%d", view->len);
    PRINT_MEMB("%d", view->ndim);
    PRINT_MEMB("%s", view->format);
    PRINT_MEMB("%d", view->itemsize);
    PRINT_MEMB("%d", view->readonly);
    PRINT_MEMB("%d", view->shape[0]);
    PRINT_MEMB("%d", view->strides[0]);
    */

    // Step 3/5
    self->bufferExportCount++;

    // Step 4/5
    view->obj = (PyObject*) self;
    Py_INCREF(view->obj);

    // Step 5/5
    return 0;
}

int JArray_getbufferproc_boolean(JPy_JArray* self, Py_buffer* view, int flags)
{
    return JArray_GetBufferProc(self, view, flags, 'Z', 1, "B");
}

int JArray_getbufferproc_char(JPy_JArray* self, Py_buffer* view, int flags)
{
    return JArray_GetBufferProc(self, view, flags, 'C', 2, "H");
}

int JArray_getbufferproc_byte(JPy_JArray* self, Py_buffer* view, int flags)
{
    return JArray_GetBufferProc(self, view, flags, 'B', 1, "b");
}

int JArray_getbufferproc_short(JPy_JArray* self, Py_buffer* view, int flags)
{
    return JArray_GetBufferProc(self, view, flags, 'S', 2, "h");
}

int JArray_getbufferproc_int(JPy_JArray* self, Py_buffer* view, int flags)
{
    return JArray_GetBufferProc(self, view, flags, 'I', 4, "i");
}

int JArray_getbufferproc_long(JPy_JArray* self, Py_buffer* view, int flags)
{
    return JArray_GetBufferProc(self, view, flags, 'J', 8, "q");
}

int JArray_getbufferproc_float(JPy_JArray* self, Py_buffer* view, int flags)
{
    return JArray_GetBufferProc(self, view, flags, 'F', 4, "f");
}

int JArray_getbufferproc_double(JPy_JArray* self, Py_buffer* view, int flags)
{
    return JArray_GetBufferProc(self, view, flags, 'D', 8, "d");
}


/*
 * Implements the releasebuffer() method the buffer protocol for JPy_JArray objects
 */
void JArray_ReleaseBufferProc(JPy_JArray* self, Py_buffer* view, char javaType)
{
    // Step 1
    self->bufferExportCount--;

    JPy_DEBUG_PRINTF("JArray_ReleaseBufferProc: buf=%p, bufferExportCount=%d\n", view->buf, self->bufferExportCount);

    // Step 2
    if (self->bufferExportCount == 0 && view->buf != NULL) {
        JNIEnv* jenv = JPy_GetJNIEnv();
        if (jenv != NULL) {
#ifdef JPy_USE_GET_PRIMITIVE_ARRAY_CRITICAL
           (*jenv)->ReleasePrimitiveArrayCritical(jenv, self->objectRef, view->buf, 0);
#else
            if (javaType == 'Z') {
                (*jenv)->ReleaseBooleanArrayElements(jenv, self->objectRef, (jboolean*) view->buf, 0);
            } else if (javaType == 'C') {
                (*jenv)->ReleaseCharArrayElements(jenv, self->objectRef, (jchar*) view->buf, 0);
            } else if (javaType == 'B') {
                (*jenv)->ReleaseByteArrayElements(jenv, self->objectRef, (jbyte*) view->buf, 0);
            } else if (javaType == 'S') {
                (*jenv)->ReleaseShortArrayElements(jenv, self->objectRef, (jshort*) view->buf, 0);
            } else if (javaType == 'I') {
                (*jenv)->ReleaseIntArrayElements(jenv, self->objectRef, (jint*) view->buf, 0);
            } else if (javaType == 'J') {
                (*jenv)->ReleaseLongArrayElements(jenv, self->objectRef, (jlong*) view->buf, 0);
            } else if (javaType == 'F') {
                (*jenv)->ReleaseFloatArrayElements(jenv, self->objectRef, (jfloat*) view->buf, 0);
            } else if (javaType == 'D') {
                (*jenv)->ReleaseDoubleArrayElements(jenv, self->objectRef, (jdouble*) view->buf, 0);
            }
#endif
        }
        view->buf = NULL;
    }

    // todo - check if we must Py_DECREF here
    //Py_DECREF(view->obj);
}

void JArray_releasebufferproc_boolean(JPy_JArray* self, Py_buffer* view)
{
    JArray_ReleaseBufferProc(self, view, 'Z');
}

void JArray_releasebufferproc_char(JPy_JArray* self, Py_buffer* view)
{
    JArray_ReleaseBufferProc(self, view, 'C');
}

void JArray_releasebufferproc_byte(JPy_JArray* self, Py_buffer* view)
{
    JArray_ReleaseBufferProc(self, view, 'B');
}

void JArray_releasebufferproc_short(JPy_JArray* self, Py_buffer* view)
{
    JArray_ReleaseBufferProc(self, view, 'S');
}

void JArray_releasebufferproc_int(JPy_JArray* self, Py_buffer* view)
{
    JArray_ReleaseBufferProc(self, view, 'I');
}

void JArray_releasebufferproc_long(JPy_JArray* self, Py_buffer* view)
{
    JArray_ReleaseBufferProc(self, view, 'J');
}

void JArray_releasebufferproc_float(JPy_JArray* self, Py_buffer* view)
{
    JArray_ReleaseBufferProc(self, view, 'F');
}

void JArray_releasebufferproc_double(JPy_JArray* self, Py_buffer* view)
{
    JArray_ReleaseBufferProc(self, view, 'D');
}


PyBufferProcs JArray_as_buffer_boolean = {
    (getbufferproc) JArray_getbufferproc_boolean,
    (releasebufferproc) JArray_releasebufferproc_boolean
};

PyBufferProcs JArray_as_buffer_char = {
    (getbufferproc) JArray_getbufferproc_char,
    (releasebufferproc) JArray_releasebufferproc_char
};

PyBufferProcs JArray_as_buffer_byte = {
    (getbufferproc) JArray_getbufferproc_byte,
    (releasebufferproc) JArray_releasebufferproc_byte
};

PyBufferProcs JArray_as_buffer_short = {
    (getbufferproc) JArray_getbufferproc_short,
    (releasebufferproc) JArray_releasebufferproc_short
};

PyBufferProcs JArray_as_buffer_int = {
    (getbufferproc) JArray_getbufferproc_int,
    (releasebufferproc) JArray_releasebufferproc_int
};

PyBufferProcs JArray_as_buffer_long = {
    (getbufferproc) JArray_getbufferproc_long,
    (releasebufferproc) JArray_releasebufferproc_long
};

PyBufferProcs JArray_as_buffer_float = {
    (getbufferproc) JArray_getbufferproc_float,
    (releasebufferproc) JArray_releasebufferproc_float
};

PyBufferProcs JArray_as_buffer_double = {
    (getbufferproc) JArray_getbufferproc_double,
    (releasebufferproc) JArray_releasebufferproc_double
};