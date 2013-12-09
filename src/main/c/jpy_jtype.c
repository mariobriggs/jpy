#include "jpy_module.h"
#include "jpy_jtype.h"
#include "jpy_jmethod.h"
#include "jpy_jobj.h"

JPy_JType* JType_New(JNIEnv* jenv, jclass classRef, jboolean resolve);
int JType_ResolveType(JNIEnv* jenv, JPy_JType* type);
int JType_InitComponentType(JNIEnv* jenv, JPy_JType* type, jboolean resolve);
int JType_InitSuperType(JNIEnv* jenv, JPy_JType* type, jboolean resolve);
int JType_ProcessClassConstructors(JNIEnv* jenv, JPy_JType* type);
int JType_ProcessClassMethods(JNIEnv* jenv, JPy_JType* type);
int JType_AddMethod(JPy_JType* type, PyObject* methodKey, JPy_JMethod* method);
JPy_ReturnDescriptor* JType_CreateReturnDescriptor(JNIEnv* jenv, jclass returnType);
JPy_ParamDescriptor* JType_CreateParamDescriptors(JNIEnv* jenv, int paramCount, jarray paramTypes);
void JType_InitParamDescriptorMethods(JPy_ParamDescriptor* paramDescriptor);


PyTypeObject* JType_GetTypeForName(const char* typeName, jboolean resolve)
{
    JNIEnv* jenv;
    const char* resourceName;
    jclass classRef;

    JPY_GET_JENV(jenv, NULL)

    if (strchr(typeName, '.') != NULL) {
        // resourceName: Replace dots '.' by slashes '/'
        char* c;
        resourceName = PyMem_New(char, strlen(typeName) + 1);
        if (resourceName == NULL) {
            PyErr_NoMemory();
            return NULL;
        }
        strcpy((char*) resourceName, typeName);
        c = (char*) resourceName;
        while ((c = strchr(c, '.')) != NULL) {
            *c = '/';
        }
    } else {
        resourceName = typeName;
    }

    if (JPy_IsDebug()) printf("JType_GetTypeForName: typeName='%s', resourceName='%s'\n", typeName, resourceName);

    classRef = (*jenv)->FindClass(jenv, resourceName);

    if (typeName != resourceName) {
        PyMem_Del((char*) resourceName);
    }

    if (classRef == NULL) {
        PyErr_SetString(PyExc_ValueError, "Java class not found");
        return NULL;
    }

    return JType_GetType(classRef, resolve);
}

/**
 * Returns a new reference.
 */
PyTypeObject* JType_GetType(jclass classRef, jboolean resolve)
{
    JNIEnv* jenv;
    PyObject* typeKey;
    JPy_JType* type;

    JPY_GET_JENV(jenv, NULL)

    typeKey = JPy_GetTypeNameString(jenv, classRef);

    // todo: add check, because the following is a dangerous cast: someone else could have put something else into JPy_Types
    type = (JPy_JType*) PyDict_GetItem(JPy_Types, typeKey);
    if (type == NULL) {

        // Create a new type instance
        type = JType_New(jenv, classRef, resolve);
        if (type == NULL) {
            Py_DECREF(typeKey);
            return NULL;
        }

        // In order to avoid infinite recursion, we have to register the new type first...
        PyDict_SetItem(JPy_Types, typeKey, (PyObject*) type);

        // ... before we can continue processing the super type ...
        if (JType_InitSuperType(jenv, type, resolve) < 0) {
            PyDict_DelItem(JPy_Types, typeKey);
            return NULL;
        }

        // ... and processing the component type.
        if (JType_InitComponentType(jenv, type, resolve) < 0) {
            PyDict_DelItem(JPy_Types, typeKey);
            return NULL;
        }

        // Finally we initialise the type's slots, so that our JObj instances behave pythonic.
        if (JType_InitSlots(type) < 0) {
            PyDict_DelItem(JPy_Types, typeKey);
            return NULL;
        }

    } else {
        Py_DECREF(typeKey);
    }

    if (JPy_IsDebug()) printf("JType_GetType: javaName='%s', resolve=%d, resolved=%d, type=%p\n", type->javaName, resolve, type->isResolved, type);

    if (!type->isResolved && resolve) {
        if (JType_ResolveType(jenv, type) < 0) {
            return NULL;
        }
    }

    return (PyTypeObject*) type;
}

/**
 * Creates a type instance of the meta type 'JType_Type'.
 * Such type instances are used as types for Java Objects in Python.
 */
JPy_JType* JType_New(JNIEnv* jenv, jclass classRef, jboolean resolve)
{
    PyTypeObject* metaType;
    JPy_JType* type;

    metaType = &JType_Type;

    type = (JPy_JType*) metaType->tp_alloc(metaType, 0);
    if (type == NULL) {
        return NULL;
    }

    type->classRef = NULL;
    type->isResolved = JNI_FALSE;
    type->isResolving = JNI_FALSE;

    type->javaName = JPy_AllocTypeNameUTF(jenv, classRef);
    if (type->javaName == NULL) {
        metaType->tp_free(type);
        return NULL;
    }
    type->typeObj.tp_name = type->javaName;

    type->classRef = (*jenv)->NewGlobalRef(jenv, classRef);
    if (type->classRef == NULL) {
        PyMem_Del(type->javaName);
        type->javaName = NULL;
        metaType->tp_free(type);
        PyErr_NoMemory();
        return NULL;
    }

    if (JPy_IsDebug()) printf("JType_New: javaName='%s', resolve=%d, type=%p\n", type->javaName, resolve, type);

    return type;
}

int JType_AssessToJBoolean(JPy_JType* type, PyObject* arg)
{
    if (PyBool_Check(arg)) return 100;
    else if (PyLong_Check(arg)) return 10;
    else return 0;
}

int JType_ConvertToJBoolean(JPy_JType* type, PyObject* arg, jvalue* value)
{
    value->z = (jboolean) (PyLong_AsLong(arg) != 0);
    return 0;
}

int JType_AssessToJByte(JPy_JType* type, PyObject* arg)
{
    if (PyLong_Check(arg)) return 100;
    else if (PyBool_Check(arg)) return 10;
    else return 0;
}

int JType_ConvertToJByte(JPy_JType* type, PyObject* arg, jvalue* value)
{
    value->b = (jbyte) PyLong_AsLong(arg);
    return 0;
}

int JType_AssessToJChar(JPy_JType* type, PyObject* arg)
{
    if (PyLong_Check(arg)) return 100;
    else if (PyBool_Check(arg)) return 10;
    else return 0;
}

int JType_ConvertToJChar(JPy_JType* type, PyObject* arg, jvalue* value)
{
    value->c = (jchar) PyLong_AsLong(arg);
    return 0;
}

int JType_AssessToJShort(JPy_JType* type, PyObject* arg)
{
    if (PyLong_Check(arg)) return 100;
    else if (PyBool_Check(arg)) return 10;
    else return 0;
}

int JType_ConvertToJShort(JPy_JType* type, PyObject* arg, jvalue* value)
{
    value->s = (jshort) PyLong_AsLong(arg);
    return 0;
}

int JType_AssessToJInt(JPy_JType* type, PyObject* arg)
{
    if (PyLong_Check(arg)) return 100;
    else if (PyBool_Check(arg)) return 10;
    else return 0;
}

int JType_ConvertToJInt(JPy_JType* type, PyObject* arg, jvalue* value)
{
    value->i = (jint) PyLong_AsLong(arg);
    return 0;
}

int JType_AssessToJLong(JPy_JType* type, PyObject* arg)
{
    if (PyLong_Check(arg)) return 100;
    else if (PyBool_Check(arg)) return 10;
    else return 0;
}

int JType_ConvertToJLong(JPy_JType* type, PyObject* arg, jvalue* value)
{
    value->j = (jlong) PyLong_AsLongLong(arg);
    return 0;
}

int JType_AssessToJFloat(JPy_JType* type, PyObject* arg)
{
    if (PyFloat_Check(arg)) return 90; // not 100, in order to give 'double' a chance
    else if (PyNumber_Check(arg)) return 50;
    else if (PyLong_Check(arg)) return 10;
    else if (PyBool_Check(arg)) return 1;
    else return 0;
}

int JType_ConvertToJFloat(JPy_JType* type, PyObject* arg, jvalue* value)
{
    value->f = (jfloat) PyFloat_AsDouble(arg);
    return 0;
}

int JType_AssessToJDouble(JPy_JType* type, PyObject* arg)
{
    if (PyFloat_Check(arg)) return 100;
    else if (PyNumber_Check(arg)) return 50;
    else if (PyLong_Check(arg)) return 10;
    else if (PyBool_Check(arg)) return 1;
    else return 0;
}

int JType_ConvertToJDouble(JPy_JType* type, PyObject* arg, jvalue* value)
{
    value->d = (jdouble) PyFloat_AsDouble(arg);
    return 0;
}

int JType_AssessToJObject(JPy_JType* type, PyObject* arg)
{
    JNIEnv* jenv;
    JPy_JType* jArgType;
    JPy_JObj* jArg;

    if (arg == Py_None) {
        // Signal it is possible, but give low priority since we cannot perform any type checks on 'None'
        return 1;
    }

    if (!JObj_Check(arg)) {
        return 0;
    }

    jArgType = (JPy_JType*) Py_TYPE(arg);
    if (jArgType == type) {
        return 100;
    }

    JPY_GET_JENV(jenv, 0)

    jArg = (JPy_JObj*) arg;
    if ((*jenv)->IsInstanceOf(jenv, jArg->objectRef, type->classRef)) {
        if (jArgType->componentType == type->componentType) {
            return 90;
        }
        if (jArgType->componentType != NULL && type->componentType != NULL) {
            // Determines whether an object of clazz1 can be safely cast to clazz2.
            if ((*jenv)->IsAssignableFrom(jenv, jArgType->componentType->classRef, type->componentType->classRef)) {
                return 80;
            }
        }
    }

    return 0;
}

int JType_ConvertToJObject(JPy_JType* type, PyObject* arg, jvalue* value)
{
    if (arg == Py_None) {
        value->l = NULL;
    } else {
        JPy_JObj* obj = (JPy_JObj*) arg;
        value->l = obj->objectRef;
    }
    return 0;
}

int JType_AssessToJString(JPy_JType* type, PyObject* arg)
{
    if (arg == Py_None) {
        // Signal it is possible, but give low priority since we cannot perform any type checks on 'None'
        return 1;
    }
    if (PyUnicode_Check(arg)) {
        return 100;
    }
    return 0;
}

int JType_ConvertToJString(JPy_JType* type, PyObject* arg, jvalue* value)
{
    JNIEnv* jenv;
    JPY_GET_JENV(jenv, -1);
    return JPy_ConvertPythonToJavaString(jenv, arg, &value->l);
}

PyObject* JType_ConvertJavaToPythonObject(JNIEnv* jenv, JPy_JType* type, jobject objectRef)
{
    // todo: check for Java wrapper types as well: java.lang.Byte, java.lang.Short, java.lang.Integer, ...
    if ((PyTypeObject*) type == JPy_JString) {
        return JPy_ConvertJavaToPythonString(jenv, objectRef);
        //return JPy_ConvertJavaToStringToPythonString(jenv, objectRef);
    } else {
        return (PyObject*) JObj_FromType(jenv, type, objectRef);
    }
}

int JType_ConvertPythonToJavaObject(JNIEnv* jenv, JPy_JType* type, PyObject* arg, jobject* objectRef)
{
    if (arg == Py_None) {
        *objectRef = NULL;
        return 0;
    }
    if (JObj_Check(arg)) {
        *objectRef = ((JPy_JObj*) arg)->objectRef;
        return 0;
    }
    if ((PyTypeObject*) type == JPy_JString && PyUnicode_Check(arg)) {
        // todo: problem of memory leak here: '**objectRef' escapes but we must actually must call jenv->DeleteLocalRef(*objectRef) some time later
        if (JPy_ConvertPythonToJavaString(jenv, arg, objectRef) < 0) {
            return -1;
        }
        return 0;
    }
    PyErr_SetString(PyExc_RuntimeError, "failed to convert Python to Java object");
    return -1;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following functions deal with type creation, initialisation, and resolution.


/**
 * Fill the type __dict__ with our Java class constructors and methods.
 * Constructors will be available using the key named __jinit__.
 * Methods will be available using their method name.
 */
int JType_ResolveType(JNIEnv* jenv, JPy_JType* type)
{
    PyTypeObject* typeObj;

    if (type->isResolved || type->isResolving) {
        return 0;
    }

    type->isResolving = JNI_TRUE;

    typeObj = (PyTypeObject*) type;
    if (typeObj->tp_base != NULL && JType_Check((PyObject*) typeObj->tp_base)) {
        JPy_JType* baseType = (JPy_JType*) typeObj->tp_base;
        if (!baseType->isResolved) {
            if (JType_ResolveType(jenv, baseType) < 0) {
                type->isResolving = JNI_FALSE;
                return -1;
            }
        }
    }

    //printf("JType_ResolveType 1\n");
    if (JType_ProcessClassConstructors(jenv, type) < 0) {
        type->isResolving = JNI_FALSE;
        return -1;
    }

    //printf("JType_ResolveType 2\n");
    if (JType_ProcessClassMethods(jenv, type) < 0) {
        type->isResolving = JNI_FALSE;
        return -1;
    }

    //printf("JType_ResolveType 3\n");
    type->isResolving = JNI_FALSE;
    type->isResolved = JNI_TRUE;
    return 0;
}


int JType_ProcessMethod(JNIEnv* jenv, JPy_JType* type, PyObject* methodKey, const char* methodName, jclass returnType, jarray paramTypes, jboolean isStatic, jmethodID mid)
{
    JPy_ParamDescriptor* paramDescriptors = NULL;
    JPy_ReturnDescriptor* returnDescriptor = NULL;
    jint paramCount;
    JPy_JMethod* jMethod;


    paramCount = (*jenv)->GetArrayLength(jenv, paramTypes);
    if (JPy_IsDebug()) printf("JType_ProcessMethod: methodName=%s, paramCount=%d, isStatic=%d, mid=%p\n", methodName, paramCount, isStatic, mid);
    if (paramCount > 0) {
        paramDescriptors = JType_CreateParamDescriptors(jenv, paramCount, paramTypes);
        if (paramDescriptors == NULL) {
            // todo: log problem
            if (JPy_IsDebug()) printf("JType_ProcessMethod: error: Java method %s rejected because an error occurred during parameter type processing\n", methodName);
            return -1;
        }
    } else {
        paramDescriptors = NULL;
    }

    if (returnType != NULL) {
        returnDescriptor = JType_CreateReturnDescriptor(jenv, returnType);
        if (returnDescriptor == NULL) {
            PyMem_Del(paramDescriptors);
            // todo: log problem
            if (JPy_IsDebug()) printf("JType_ProcessMethod: error: Java method %s rejected because an error occurred during return type processing\n", methodName);
            return -1;
        }
    } else {
        returnDescriptor = NULL;
    }

    jMethod = JMethod_New(methodKey, paramCount, paramDescriptors, returnDescriptor, isStatic, mid);
    if (jMethod == NULL) {
        PyMem_Del(paramDescriptors);
        PyMem_Del(returnDescriptor);
        // todo: log problem
        if (JPy_IsDebug()) printf("JType_ProcessMethod: error: Java method %s rejected because an error occurred during method instantiation\n", methodName);
        return -1;
    }

    JType_AddMethod(type, methodKey, jMethod);
    return 0;
}

int JType_InitComponentType(JNIEnv* jenv, JPy_JType* type, jboolean resolve)
{
    jclass componentTypeRef;

    componentTypeRef = (jclass) (*jenv)->CallObjectMethod(jenv, type->classRef, JPy_Class_GetComponentType_MID);
    if (componentTypeRef != NULL) {
        type->componentType = (JPy_JType*) JType_GetType(componentTypeRef, resolve);
        if (type->componentType == NULL) {
            return -1;
        }
        Py_INCREF(type->componentType);
    } else {
        type->componentType = NULL;
    }

    return 0;
}

int JType_InitSuperType(JNIEnv* jenv, JPy_JType* type, jboolean resolve)
{
    jclass superClassRef;

    superClassRef = (*jenv)->GetSuperclass(jenv, type->classRef);
    if (superClassRef != NULL) {
        type->superType = (JPy_JType*) JType_GetType(superClassRef, resolve);
        if (type->superType == NULL) {
            return -1;
        }
        Py_INCREF(type->superType);
    } else {
        type->superType = NULL;
    }

    return 0;
}


int JType_ProcessClassConstructors(JNIEnv* jenv, JPy_JType* type)
{
    jclass classRef;
    jobject constructors;
    jobject constructor;
    jobject parameterTypes;
    jint modifiers;
    jint constrCount;
    jint i;
    jboolean isPublic;
    jmethodID mid;
    PyObject* methodKey;

    classRef = type->classRef;
    methodKey = Py_BuildValue("s", JPy_JINIT_ATTR_NAME);
    constructors = (*jenv)->CallObjectMethod(jenv, classRef, JPy_Class_GetDeclaredConstructors_MID);
    constrCount = (*jenv)->GetArrayLength(jenv, constructors);

    if (JPy_IsDebug()) printf("JType_ProcessClassConstructors: constrCount=%d\n", constrCount);

    for (i = 0; i < constrCount; i++) {
        constructor = (*jenv)->GetObjectArrayElement(jenv, constructors, i);
        modifiers = (*jenv)->CallIntMethod(jenv, constructor, JPy_Constructor_GetModifiers_MID);
        isPublic = (modifiers & 0x0001) != 0;
        if (isPublic) {
            parameterTypes = (*jenv)->CallObjectMethod(jenv, constructor, JPy_Constructor_GetParameterTypes_MID);
            mid = (*jenv)->FromReflectedMethod(jenv, constructor);
            JType_ProcessMethod(jenv, type, methodKey, JPy_JINIT_ATTR_NAME, NULL, parameterTypes, 1, mid);
        }
    }
    return 0;
}


int JType_ProcessClassMethods(JNIEnv* jenv, JPy_JType* type)
{
    jclass classRef;
    jobject methods;
    jobject method;
    jobject methodNameStr;
    jobject returnType;
    jobject parameterTypes;
    jint modifiers;
    jint methodCount;
    jint i;
    jboolean isStatic;
    jboolean isPublic;
    const char * methodName;
    jmethodID mid;
    PyObject* methodKey;

    classRef = type->classRef;

    methods = (*jenv)->CallObjectMethod(jenv, classRef, JPy_Class_GetDeclaredMethods_MID);
    methodCount = (*jenv)->GetArrayLength(jenv, methods);

    if (JPy_IsDebug()) printf("JType_ProcessClassMethods: methodCount=%d\n", methodCount);

    for (i = 0; i < methodCount; i++) {
        method = (*jenv)->GetObjectArrayElement(jenv, methods, i);
        modifiers = (*jenv)->CallIntMethod(jenv, method, JPy_Method_GetModifiers_MID);
        // see http://docs.oracle.com/javase/6/docs/api/constant-values.html#java.lang.reflect.Modifier.PUBLIC
        isPublic = (modifiers & 0x0001) != 0;
        isStatic = (modifiers & 0x0008) != 0;
        if (isPublic) {
            methodNameStr = (*jenv)->CallObjectMethod(jenv, method, JPy_Method_GetName_MID);
            returnType = (*jenv)->CallObjectMethod(jenv, method, JPy_Method_GetReturnType_MID);
            parameterTypes = (*jenv)->CallObjectMethod(jenv, method, JPy_Method_GetParameterTypes_MID);
            mid = (*jenv)->FromReflectedMethod(jenv, method);

            methodName = (*jenv)->GetStringUTFChars(jenv, methodNameStr, NULL);
            methodKey = Py_BuildValue("s", methodName);
            JType_ProcessMethod(jenv, type, methodKey, methodName, returnType, parameterTypes, isStatic, mid);

            (*jenv)->ReleaseStringUTFChars(jenv, methodNameStr, methodName);
        }
    }
    return 0;
}

int JType_AddMethod(JPy_JType* type, PyObject* methodKey, JPy_JMethod* method)
{
    PyObject* typeDict;
    PyObject* methodValue;
    JPy_JOverloadedMethod* overloadedMethod;

    typeDict = type->typeObj.tp_dict;

    methodValue = PyDict_GetItem(typeDict, methodKey);
    if (methodValue == NULL) {
        overloadedMethod = JOverloadedMethod_New(type, methodKey, method);
        return PyDict_SetItem(typeDict, methodKey, (PyObject*) overloadedMethod);
    } else if (PyObject_TypeCheck(methodValue, &JOverloadedMethod_Type)) {
        overloadedMethod = (JPy_JOverloadedMethod*) methodValue;
        return JOverloadedMethod_AddMethod(overloadedMethod, method);
    }

    return -1;
}

JPy_ReturnDescriptor* JType_CreateReturnDescriptor(JNIEnv* jenv, jclass returnClass)
{
    JPy_ReturnDescriptor* returnDescriptor;
    PyTypeObject* type;

    returnDescriptor = PyMem_New(JPy_ReturnDescriptor, 1);
    if (returnDescriptor == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    type = JType_GetType(returnClass, JNI_FALSE);
    if (type == NULL) {
        return NULL;
    }

    returnDescriptor->type = (JPy_JType*) type;
    Py_INCREF((PyObject*) type);

    // if (JPy_IsDebug()) printf("JType_ProcessReturnType: type->tp_name='%s',
    //                           type=%p\n", type->tp_name, type);

    return returnDescriptor;
}


JPy_ParamDescriptor* JType_CreateParamDescriptors(JNIEnv* jenv, int paramCount, jarray paramClasses)
{
    JPy_ParamDescriptor* paramDescriptors;
    JPy_ParamDescriptor* paramDescriptor;
    PyTypeObject* type;
    jclass paramClass;
    int i;

    paramDescriptors = PyMem_New(JPy_ParamDescriptor, paramCount);
    if (paramDescriptors == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    //printf("JType_ProcessParamTypes 1; paramCount=%d\n", paramCount);
    for (i = 0; i < paramCount; i++) {
        paramClass = (*jenv)->GetObjectArrayElement(jenv, paramClasses, i);
        paramDescriptor = paramDescriptors + i;

        type = JType_GetType(paramClass, JNI_FALSE);
        if (type == NULL) {
            return NULL;
        }

        paramDescriptor->type = (JPy_JType*) type;
        Py_INCREF((PyObject*) paramDescriptor->type);

        JType_InitParamDescriptorMethods(paramDescriptor);

        // if (JPy_IsDebug()) printf("JType_ProcessParamTypes: parameters[%d]: type->tp_name='%s', type->classRef=%p\n",
        //                            i, type->tp_name, paramDescriptor->type->classRef);
    }

    return paramDescriptors;
}

void JType_InitParamDescriptorMethods(JPy_ParamDescriptor* paramDescriptor)
{
    PyTypeObject* type = (PyTypeObject*) paramDescriptor->type;

    //jboolean isPrimitive;
    //isPrimitive = (*jenv)->CallBooleanMethod(jenv, classRef, JPy_Class_IsPrimitive_MID);

    if (type == JPy_JVoid) {
        paramDescriptor->assessToJValue = NULL;
        paramDescriptor->convertToJValue = NULL;
    } else if (type == JPy_JBoolean) {
        paramDescriptor->assessToJValue = JType_AssessToJBoolean;
        paramDescriptor->convertToJValue = JType_ConvertToJBoolean;
    } else if (type == JPy_JByte) {
        paramDescriptor->assessToJValue = JType_AssessToJByte;
        paramDescriptor->convertToJValue = JType_ConvertToJByte;
    } else if (type == JPy_JChar) {
        paramDescriptor->assessToJValue = JType_AssessToJChar;
        paramDescriptor->convertToJValue = JType_ConvertToJChar;
    } else if (type == JPy_JShort) {
        paramDescriptor->assessToJValue = JType_AssessToJShort;
        paramDescriptor->convertToJValue = JType_ConvertToJShort;
    } else if (type == JPy_JInt) {
        paramDescriptor->assessToJValue = JType_AssessToJInt;
        paramDescriptor->convertToJValue = JType_ConvertToJInt;
    } else if (type == JPy_JLong) {
        paramDescriptor->assessToJValue = JType_AssessToJLong;
        paramDescriptor->convertToJValue = JType_ConvertToJLong;
    } else if (type == JPy_JFloat) {
        paramDescriptor->assessToJValue = JType_AssessToJFloat;
        paramDescriptor->convertToJValue = JType_ConvertToJFloat;
    } else if (type == JPy_JDouble) {
        paramDescriptor->assessToJValue = JType_AssessToJDouble;
        paramDescriptor->convertToJValue = JType_ConvertToJDouble;
    } else if (type == JPy_JString) {
        paramDescriptor->assessToJValue = JType_AssessToJString;
        paramDescriptor->convertToJValue = JType_ConvertToJString;
    //} else if (type == JPy_JMap) {
    //} else if (type == JPy_JList) {
    //} else if (type == JPy_JSet) {
    } else {
        paramDescriptor->assessToJValue = JType_AssessToJObject;
        paramDescriptor->convertToJValue = JType_ConvertToJObject;
    }

}








/**
 * The JType's tp_repr slot.
 */
PyObject* JType_repr(JPy_JType* self)
{
    printf("JType_repr: self=%p\n", self);
    return PyUnicode_FromFormat("%s(%p)",
                                self->javaName,
                                self->classRef);
}

/**
 * The JType's tp_str slot.
 */
PyObject* JType_str(JPy_JType* self)
{
    JNIEnv* jenv;
    jstring strJObj;
    PyObject* strPyObj;
    jboolean isCopy;
    const char * utfChars;

    JPY_GET_JENV(jenv, NULL)

    printf("JType_str: self=%p\n", self);

    strJObj = (*jenv)->CallObjectMethod(jenv, self->classRef, JPy_Object_ToString_MID);
    utfChars = (*jenv)->GetStringUTFChars(jenv, strJObj, &isCopy);
    strPyObj = PyUnicode_FromFormat("%s", utfChars);
    (*jenv)->ReleaseStringUTFChars(jenv, strJObj, utfChars);
    return strPyObj;
}

/**
 * The JType's tp_dealloc slot.
 */
void JType_dealloc(JPy_JType* self)
{
    JNIEnv* jenv = JPy_GetJNIEnv();

    printf("JType_dealloc: self->javaName='%s', self->classRef=%p\n", self->javaName, self->classRef);

    PyMem_Del(self->javaName);
    self->javaName = NULL;

    if (jenv != NULL && self->classRef != NULL) {
        (*jenv)->DeleteGlobalRef(jenv, self->classRef);
        self->classRef = NULL;
    }

    Py_DECREF(self->superType);
    self->superType = NULL;

    Py_DECREF(self->componentType);
    self->componentType = NULL;

    Py_TYPE(self)->tp_free((PyObject*) self);
}

/**
 * The JType's JType_getattro slot.
 */
PyObject* JType_getattro(JPy_JType* self, PyObject* name)
{
    if (!self->isResolved && !self->isResolving) {
        JNIEnv* jenv;
        JPY_GET_JENV(jenv, NULL);
        JType_ResolveType(jenv, self);
    }

    return PyObject_GenericGetAttr((PyObject*) self, name);
}


/**
 * Implements the BeamPy_JObjectType class singleton.
 */
PyTypeObject JType_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "jpy.JType",                 /* tp_name */
    sizeof (JPy_JType),          /* tp_basicsize */
    0,                           /* tp_itemsize */
    (destructor) JType_dealloc,  /* tp_dealloc */
    NULL,                         /* tp_print */
    NULL,                         /* tp_getattr */
    NULL,                         /* tp_setattr */
    NULL,                         /* tp_reserved */
    (reprfunc) JType_repr,        /* tp_repr */
    NULL,                         /* tp_as_number */
    NULL,                         /* tp_as_sequence */
    NULL,                         /* tp_as_mapping */
    NULL,                         /* tp_hash  */
    NULL,                         /* tp_call */
    (reprfunc) JType_str,         /* tp_str */
    (getattrofunc)JType_getattro, /* tp_getattro */
    NULL,                         /* tp_setattro */
    NULL,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /* tp_flags */
    "Java Meta Type",             /* tp_doc */
    NULL,                         /* tp_traverse */
    NULL,                         /* tp_clear */
    NULL,                         /* tp_richcompare */
    0,                            /* tp_weaklistoffset */
    NULL,                         /* tp_iter */
    NULL,                         /* tp_iternext */
    NULL,                         /* tp_methods */
    NULL,                         /* tp_members */
    NULL,                         /* tp_getset */
    NULL,                         /* tp_base */
    NULL,                         /* tp_dict */
    NULL,                         /* tp_descr_get */
    NULL,                         /* tp_descr_set */
    0,                            /* tp_dictoffset */
    (initproc) NULL,              /* tp_init */
    NULL,                         /* tp_alloc */
    (newfunc) NULL,               /* tp_new=NULL --> JType instances cannot be created from Python. */
};

