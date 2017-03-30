#include <cstring>
#include <math.h>
#include <stdexcept>

class jsonArray;

class jsonObject;

class jsonValue;

class jsonKeyValue;


/**
 *  在指定区域中查找字符，跳过引号内及括号内的区域（但是会查找左引号和左括号）。
 *  查找的起点应该在引号及括号外部。用于查找 " , . : [ ] { } 等标志字符
 *  " k e y " : { " a b c " : 1 1 1 }
 *  + - - - - + + - - - - - - - - - -
 *  上图的示例中，加号表示的位可以被查找到
 * */
int find_char(const char *raw, int len, char c);

/**
 *  在指定区域中查找引号（" ），返回第一个的位置，通常用于查找key的右引号
 * */
int find_quote(const char *raw, int len);

bool check_if_num(const char *raw, int len,bool exponential=true);

double get_num(const char *raw, int len,bool exponential=true);

int num_to_char(double number, char *buff = NULL);

class jsonValue {
    friend class jsonObject;
    friend class jsonArray;
    friend class jsonKeyValue;
public:
    jsonValue(const char *raw, int len);

    jsonValue();

    ~jsonValue();

private:
    int toString(char *buff = NULL);

    static const char *get_type_name(int type);

    static const int TYPE_ERROR = 0;
    static const int TYPE_NUMBER = 1;
    static const int TYPE_STRING = 2;
    static const int TYPE_BOOL = 3;
    static const int TYPE_NULL = 4;
    static const int TYPE_OBJECT = 5;
    static const int TYPE_ARRAY = 6;

    char *string_value;
    double number_value;
    bool bool_value;
    jsonObject *object;
    jsonArray *array;

    int value_type;

    jsonValue *next;
};


class jsonKeyValue {
    friend class jsonArray;
    friend class jsonObject;
    friend class jsonValue;
public:
    jsonKeyValue(const char *raw, int len);

    jsonKeyValue();

    ~jsonKeyValue();

private:
    int toString(char *buff = NULL);

    jsonKeyValue *next;
    char *key;
    jsonValue *value;
};


class jsonObject {
    friend class jsonValue;
    friend class jsonKeyValue;
    friend class jsonArray;
public:
    jsonObject(const char *_raw="{}");

    jsonObject(const char *_raw, int len);

    ~jsonObject();

    bool has(const char *key);

    const char *getString(const char *key);

    double getDouble(const char *key);

    int getInt(const char *key);

    long getLong(const char *key);

    bool getBool(const char *key);

    jsonObject *getJsonObject(const char *key);

    jsonArray *getJsonArray(const char *key);

    jsonObject *put(const char *key, const char *str);

    jsonObject *put(const char *key, double number);

    jsonObject *put(const char *key, int number);

    jsonObject *put(const char *key, bool b);

    jsonObject *put(const char *key, jsonObject *obj);

    jsonObject *put(const char *key, jsonArray *arr);

    const char *toString();

private:
    int toString(char *buff);

    void process_raw(int raw_len);

    int copy_raw(const char *_raw, int len);

    jsonKeyValue *get(const char *key);

    jsonValue *put(const char *key);

    const char * no_value_for(const char *key);

    const char * value_convert_error(jsonKeyValue *json,const char *aim_type);

    char *error_msg;
    char *raw;
    jsonKeyValue *first;
};


class jsonArray {
    friend class jsonValue;
    friend class jsonKeyValue;
    friend class jsonObject;
public:
    jsonArray(const char *_raw="[]");

    jsonArray(const char *_raw, int len);

    ~jsonArray();

    const char *getString(int index);

    double getDouble(int index);

    int getInt(int index);

    long getLong(int index);

    bool getBool(int index);

    jsonObject *getJsonObject(int index);

    jsonArray *getJsonArray(int index);

    jsonArray *put(const char *str);

    jsonArray *put(double number);

    jsonArray *put(int number);

    jsonArray *put(bool b);

    jsonArray *put(jsonObject *obj);

    jsonArray *put(jsonArray *arr);

    const char *toString();

    int toString(char *buff);

    int length();

private:
    jsonValue *get(int index);

    jsonValue *put();

    void process_raw(int raw_len);

    int copy_raw(const char *_raw, int len);

    const char * no_value_for(int index);

    const char *value_convert_error(jsonValue *json,int index ,const char *aim_type);

    char *error_msg;
    char *raw;
    jsonValue *first;
};