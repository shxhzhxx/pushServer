#include "json.h"


int find_char(const char *raw, int len, char c) {
    int brace = 0;
    bool quote = false;
    for (int i = 0; i < len; ++i) {
        if (!quote && brace == 0 && raw[i] == c) {
            return i;
        }
        if (!quote && raw[i] == '{' || raw[i] == '[') {
            ++brace;
        }
        if (!quote && raw[i] == '}' || raw[i] == ']') {
            --brace;
        }
        if (raw[i] == '\"') {
            quote = !quote;
        }
        if (brace < 0) {
            return -1;
        }
    }
    return -1;
}

int find_quote(const char *raw, int len) {
    for (int i = 0; i < len; ++i) {
        if (raw[i] == '"') {
            return i;
        }
    }
    return -1;
}

bool check_if_num(const char *raw, int len ,bool exponential) {
    int i = 0;
    int decimal = find_char(raw, len, '.');
    int e_i=-1;
    if(exponential && ((e_i=find_char(raw,len,'e'))!=-1 || (e_i=find_char(raw,len,'E'))!=-1)){
        if(len>e_i+1){
            if(raw[e_i+1]=='+'){
                if(len>e_i+2){
                    if(!check_if_num(raw+e_i+2,len-e_i-2,false)){
                        return false;
                    }
                }else{
                    return false;
                }
            }else{
                if(!check_if_num(raw+e_i+1,len-e_i-1,false)){
                    return false;
                }
            }
        }else{
            return false;
        }
        len=e_i;
    }
    if (raw[0] == '-') {
        ++i;
    }
    if (decimal == i) {
        return false;
    }
    while (i < len) {
        if (i == decimal) {
            ++i;
            continue;
        }
        if (raw[i] < '0' || raw[i] > '9') {
            return false;
        }
        ++i;
    }
    return true;
}

double get_num(const char *raw, int len ,bool exponential) {
    double result = 0;
    int i = 0;
    int decimal = find_char(raw, len, '.');
    int e_i=-1;
    int e;
    if(exponential && ((e_i=find_char(raw,len,'e'))!=-1 || (e_i=find_char(raw,len,'E'))!=-1)){
        if(len>e_i+1){
            if(raw[e_i+1]=='+'){
                if(len>e_i+2){
                    e=get_num(raw+e_i+2,len-e_i-2,false);
                }else{
                    throw std::runtime_error("invalid param for get_num");
                }
            }else{
                e=get_num(raw+e_i+1,len-e_i-1,false);
            }
        }else{
            throw std::runtime_error("invalid param for get_num");
        }
        len=e_i;
    }
    if (raw[0] == '-') {
        ++i;
    }
    if (decimal == i) {
        throw std::runtime_error("invalid param for get_num");
    }
    while (i < len) {
        if (i == decimal) {
            ++i;
            continue;
        }
        if (raw[i] < '0' || raw[i] > '9') {
            throw std::runtime_error("invalid param for get_num");
        }
        int power = decimal == -1 ? len - i - 1 : decimal > i ? decimal - i - 1 : decimal - i;
        result += (raw[i] - '0') * pow(10, power);
        ++i;
    }
    if (raw[0] == '-') {
        result = -result;
    }
    if(e_i!=-1){
        result=result*pow(10, e);
    }
    return result;
}

int num_to_char(double number, char *buff) {

    int i = 0;
    if (number < 0) {
        if (buff) { buff[0] = '-'; }
        ++i;
        number = -number;
    }
    double n = 1;
    while (n * 10 <= number) { n *= 10; }
    while (number > 0.000001) { //浮点计算有误差
        int a = 0;//系数
        while (number >= (a + 1) * n - 0.000001) {
            ++a;
        }
        if (buff) { buff[i] = (char) ('0' + a); }
        ++i;
        number -= a * n;
        if (1 == n && number > 0.000001) {
            if (buff) { buff[i] = '.'; }
            ++i;
        }
        n /= 10;
    }
    while (n >= 1) {
        if (buff) { buff[i] = '0'; }
        ++i;
        n /= 10;
    }
    return i;
}

// jsonValue member   =================================================================
jsonValue::~jsonValue() {
    delete[]string_value;
    delete object;
    delete array;
    delete next;
}

jsonValue::jsonValue() : value_type(TYPE_ERROR), string_value(0), number_value(0), bool_value(false), object(0),
                         array(0), next(0) {}

jsonValue::jsonValue(const char *raw, int len) : value_type(TYPE_ERROR), string_value(0), number_value(0),
                                                 bool_value(false), object(0), array(0), next(0) {
    if (len >= 1 && raw[0] == '\"') {//string
        int i = find_quote(raw + 1, len - 1);
        if (i == -1) {
            throw std::runtime_error("invalid json format");
        }
        string_value = new char[i + 1]();
        memcpy(string_value, raw + 1, i);
        value_type = TYPE_STRING;
        return;
    }
    if (len >= 1 && check_if_num(raw, len)) {//int
        number_value = get_num(raw, len);
        value_type = TYPE_NUMBER;
        string_value = new char[len + 1]();
        memcpy(string_value, raw, len);
        return;
    }
    if (len >= 1 && raw[0] == '{') {//object
        if (raw[len - 1] != '}') {
            throw std::runtime_error("invalid json format");
        }
        object = new jsonObject(raw, len);
        value_type = TYPE_OBJECT;
        string_value = new char[len + 1]();
        memcpy(string_value, raw, len);
        return;
    }
    if (len >= 1 && raw[0] == '[') {//array
        if (raw[len - 1] != ']') {
            throw std::runtime_error("invalid json format");
        }
        array = new jsonArray(raw, len);
        value_type = TYPE_ARRAY;
        string_value = new char[len + 1]();
        memcpy(string_value, raw, len);
        return;
    }

    if (len == 4 && memcmp("true", raw, 4) == 0) {
        bool_value = true;
        value_type = TYPE_BOOL;
        string_value = new char[len + 1]();
        memcpy(string_value, raw, len);
        return;
    }
    if (len == 5 && memcmp("false", raw, 5) == 0) {
        bool_value = false;
        value_type = TYPE_BOOL;
        string_value = new char[len + 1]();
        memcpy(string_value, raw, len);
        return;
    }
    if (len == 4 && memcmp("null", raw, 4) == 0) {
        value_type = TYPE_NULL;
        string_value = new char[len + 1]();
        memcpy(string_value, raw, len);
        return;
    }
    throw std::runtime_error("invalid json format");
}

int jsonValue::toString(char *buff) {
    size_t len;
    switch (value_type) {
        case jsonValue::TYPE_STRING:
            len = strlen(string_value);
            if (buff) {
                buff[0] = '"';
                strncpy(buff + 1, string_value, len);
                buff[len + 1] = '"';
            }
            return (int) len + 2;
        case jsonValue::TYPE_NUMBER:
            return num_to_char(number_value, buff);
        case jsonValue::TYPE_BOOL:
            if (bool_value) {
                if (buff) { strncpy(buff, "true", 4); }
                return 4;
            } else {
                if (buff) { strncpy(buff, "false", 5); }
                return 5;
            }
        case jsonValue::TYPE_NULL:
            if (buff) { strncpy(buff, "null", 4); }
            return 4;
        case jsonValue::TYPE_OBJECT:
            return object->toString(buff);
        case jsonValue::TYPE_ARRAY:
            return array->toString(buff);
        default:
            return 0;
    }
}

const char *jsonValue::get_type_name(int type) {
    switch (type) {
        case TYPE_NUMBER:
            return "number";
        case TYPE_STRING:
            return "string";
        case TYPE_BOOL:
            return "bool";
        case TYPE_NULL:
            return "null";
        case TYPE_ARRAY:
            return "jsonArray";
        case TYPE_OBJECT:
            return "jsonObject";
        case TYPE_ERROR:
            return "error";
        default:
            return "";
    }
}


// jsonKeyValue member   =================================================================
jsonKeyValue::jsonKeyValue(const char *raw, int len) : key(0), value(0), next(0) {
    int i;
    if (raw[0] != '\"' || (i = find_quote(raw + 1, len - 1)) == -1) {//error
        throw std::runtime_error("invalid json format");
    }
    i = i + 1;
    key = new char[i];
    memcpy(key, raw + 1, i - 1);
    key[i - 1] = 0;
    if (i + 1 >= len || raw[i + 1] != ':') {//error
        throw std::runtime_error("invalid json format");
    }
    value = new jsonValue(raw + i + 2, len - i - 2);
}

jsonKeyValue::jsonKeyValue() : key(0), value(0), next(0) {}

jsonKeyValue::~jsonKeyValue() {
    delete[]key;
    delete value;
    delete next;
}

int jsonKeyValue::toString(char *buff) {
    size_t key_len = strlen(key);
    if (buff) {
        buff[0] = '"';
        strncpy(buff + 1, key, key_len);
        buff[key_len + 1] = '"';
        buff[key_len + 2] = ':';
        return (int) (key_len + 3 + value->toString(buff + key_len + 3));
    } else {
        return (int) (key_len + 3 + value->toString(buff));
    }
}



// jsonObject member   =================================================================

jsonObject::jsonObject(const char *_raw) : raw(0), first(0), error_msg(0) {
    process_raw(copy_raw(_raw, strlen(_raw)));
}

jsonObject::jsonObject(const char *_raw, int len) : raw(0), first(0), error_msg(0) {
    process_raw(copy_raw(_raw, len));
}

jsonObject::~jsonObject() {
    delete[]raw;
    delete[]error_msg;
    delete first;
}

void jsonObject::process_raw(int raw_len) {
    delete first;
    if (!raw || raw[0] != '{') {//error
        throw std::runtime_error("invalid json format");
    }
    int i = 1;
    int f;
    if ((f = find_char(raw + i, raw_len - i, ',')) == -1 && (f = find_char(raw + i, raw_len - i, '}')) == -1) {//error
        throw std::runtime_error("invalid json format");
    }
    if(f>0){
        first = new jsonKeyValue(raw + i, f);
        jsonKeyValue *end = first;
        i += f;
        while (i + 1 < raw_len && raw[i] == ',') {
            i = i + 1;
            if ((f = find_char(raw + i, raw_len - i, ',')) == -1 &&
                (f = find_char(raw + i, raw_len - i, '}')) == -1) {//error
                throw std::runtime_error("invalid json format");
            }
            jsonKeyValue *new_p = new jsonKeyValue(raw + i, f);
            jsonKeyValue *find_p = get(new_p->key);
            if (find_p) {
                delete find_p->value;
                find_p->value = new_p->value;
                new_p->value = 0;
                delete new_p;
            } else {
                end->next = new_p;
                end = end->next;
            }
            i += f;
        }
    }
    if (i + 1 != raw_len || raw[i] != '}') {//error
        throw std::runtime_error("invalid json format");
    }
}

const char *jsonObject::no_value_for(const char *key) {
    delete[]error_msg;
    error_msg = new char[14 + strlen(key)];
    strcpy(error_msg, "no value for ");
    return strcat(error_msg, key);
}

const char *jsonObject::value_convert_error(jsonKeyValue *json, const char *aim_type) {
    delete[]error_msg;
    error_msg = new char[43 + strlen(json->value->string_value) + strlen(json->key) +
                         strlen(jsonValue::get_type_name(json->value->value_type)) + strlen(aim_type) + 1]();

    strcpy(error_msg, "Value ");
    strcat(error_msg, json->value->string_value);
    strcat(error_msg, " at ");
    strcat(error_msg, json->key);
    strcat(error_msg, " of type ");
    strcat(error_msg, jsonValue::get_type_name(json->value->value_type));
    strcat(error_msg, " cannot be converted to ");
    return strcat(error_msg, aim_type);

//    sprintf(error_msg,"Value %s at %s of type %s cannot be converted to %s",json->value->string_value,json->key,jsonValue::get_type_name(json->value->value_type),aim_type);
//    return error_msg;
}

bool jsonObject::has(const char *key){
    return get(key)!=NULL;
}

const char *jsonObject::getString(const char *key) {
    jsonKeyValue *item = get(key);
    if (item && item->value) {
        switch (item->value->value_type) {
            case jsonValue::TYPE_STRING:
            case jsonValue::TYPE_NUMBER:
            case jsonValue::TYPE_BOOL:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                return item->value->string_value;
            default:
                throw std::runtime_error(no_value_for(key));
        }
    }
    throw std::runtime_error(no_value_for(key));
}

double jsonObject::getDouble(const char *key) {
    jsonKeyValue *item = get(key);
    if (item && item->value) {
        switch (item->value->value_type) {
            case jsonValue::TYPE_NUMBER:
                return item->value->number_value;
            case jsonValue::TYPE_STRING:
                if (check_if_num(item->value->string_value, strlen(item->value->string_value))) {
                    return get_num(item->value->string_value, strlen(item->value->string_value));
                }
            case jsonValue::TYPE_BOOL:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                throw std::runtime_error(value_convert_error(item, "double"));
            default:
                throw std::runtime_error(no_value_for(key));
        }
    }
    throw std::runtime_error(no_value_for(key));
}

int jsonObject::getInt(const char *key) {
    jsonKeyValue *item = get(key);
    if (item && item->value) {
        switch (item->value->value_type) {
            case jsonValue::TYPE_NUMBER:
                return (int) item->value->number_value;
            case jsonValue::TYPE_STRING:
                if (check_if_num(item->value->string_value, strlen(item->value->string_value))) {
                    return (int) get_num(item->value->string_value, strlen(item->value->string_value));
                }
            case jsonValue::TYPE_BOOL:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                throw std::runtime_error(value_convert_error(item, "int"));
            default:
                throw std::runtime_error(no_value_for(key));
        }
    }
    throw std::runtime_error(no_value_for(key));
}

long jsonObject::getLong(const char *key) {
    jsonKeyValue *item = get(key);
    if (item && item->value) {
        switch (item->value->value_type) {
            case jsonValue::TYPE_NUMBER:
                return (long) item->value->number_value;
            case jsonValue::TYPE_STRING:
                if (check_if_num(item->value->string_value, strlen(item->value->string_value))) {
                    return (long) get_num(item->value->string_value, strlen(item->value->string_value));
                }
            case jsonValue::TYPE_BOOL:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                throw std::runtime_error(value_convert_error(item, "long"));
            default:
                throw std::runtime_error(no_value_for(key));
        }
    }
    throw std::runtime_error(no_value_for(key));
}

bool jsonObject::getBool(const char *key) {
    jsonKeyValue *item = get(key);
    if (item && item->value) {
        switch (item->value->value_type) {
            case jsonValue::TYPE_BOOL:
                return item->value->bool_value;
            case jsonValue::TYPE_STRING:
                if (strcmp(item->value->string_value, "true") == 0) {
                    return true;
                } else if (strcmp(item->value->string_value, "false") == 0) {
                    return false;
                }
            case jsonValue::TYPE_NUMBER:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                throw std::runtime_error(value_convert_error(item, "bool"));
            default:
                throw std::runtime_error(no_value_for(key));
        }
    }
    throw std::runtime_error(no_value_for(key));
}

jsonObject *jsonObject::getJsonObject(const char *key) {
    jsonKeyValue *item = get(key);
    if (item && item->value) {
        if (item->value->value_type == jsonValue::TYPE_OBJECT) {
            return item->value->object;
        }
        throw std::runtime_error(value_convert_error(item, "jsonObject"));
    }
    throw std::runtime_error(no_value_for(key));
}

jsonArray *jsonObject::getJsonArray(const char *key) {
    jsonKeyValue *item = get(key);
    if (item && item->value) {
        if (item->value->value_type == jsonValue::TYPE_ARRAY) {
            return item->value->array;
        }
        throw std::runtime_error(value_convert_error(item, "jsonArray"));
    }
    throw std::runtime_error(no_value_for(key));
}

jsonKeyValue *jsonObject::get(const char *key) {
    jsonKeyValue *item = first;
    while (item) {
        if (strcmp(item->key, key) == 0) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

int jsonObject::copy_raw(const char *_raw, int len) {
    int raw_len = 0;
    bool quote = false;
    for (int i = 0; i < len; ++i) {
        if (_raw[i] == '\"') {
            quote = !quote;
        }
        if (quote || _raw[i] != ' ') {
            ++raw_len;
        }
    }
    delete[]raw;
    raw = new char[raw_len];
    int raw_index = 0;
    quote = false;
    for (int i = 0; i < len; ++i) {
        if (_raw[i] == '\"') {
            quote = !quote;
        }
        if (quote || _raw[i] != ' ') {
            raw[raw_index] = _raw[i];
            ++raw_index;
        }
    }
    return raw_len;
}

jsonObject *jsonObject::put(const char *key, const char *str) {
    jsonValue *value = put(key);
    value->value_type = jsonValue::TYPE_STRING;
    delete[]value->string_value;
    value->string_value = new char[strlen(str) + 1]();
    strcpy(value->string_value, str);
    return this;
}

jsonObject *jsonObject::put(const char *key, double number) {
    jsonValue *value = put(key);
    value->value_type = jsonValue::TYPE_NUMBER;
    value->number_value = number;
    delete[]value->string_value;
    int len = num_to_char(number);
    value->string_value = new char[len + 1]();
    num_to_char(number, value->string_value);
    return this;
}

jsonObject *jsonObject::put(const char *key, int number) {
    return put(key, (double) number);
}

jsonObject *jsonObject::put(const char *key, bool b) {
    jsonValue *value = put(key);
    value->value_type = jsonValue::TYPE_BOOL;
    value->bool_value = b;
    delete[]value->string_value;
    if (b) {
        value->string_value = new char[5]();
        strcpy(value->string_value, "true");
    } else {
        value->string_value = new char[6]();
        strcpy(value->string_value, "false");
    }
    return this;
}

jsonObject *jsonObject::put(const char *key, jsonObject *obj) {
    jsonValue *value = put(key);
    value->value_type = jsonValue::TYPE_OBJECT;
    value->object = obj;
    delete[]value->string_value;
    value->string_value = new char[obj->toString(NULL) + 1]();
    obj->toString(value->string_value);
    return this;
}

jsonObject *jsonObject::put(const char *key, jsonArray *arr) {
    jsonValue *value = put(key);
    value->value_type = jsonValue::TYPE_ARRAY;
    value->array = arr;
    delete[]value->string_value;
    value->string_value = new char[arr->toString(NULL) + 1]();
    arr->toString(value->string_value);
    return this;
}

jsonValue *jsonObject::put(const char *key) {
    if (first) {
        jsonKeyValue *p = first;
        if (strcmp(key, p->key) == 0) {
            return p->value;
        }
        while (p->next) {
            p = p->next;
            if (strcmp(key, p->key) == 0) {
                return p->value;
            }
        }
        jsonKeyValue *new_item = new jsonKeyValue();
        new_item->key = new char[strlen(key) + 1];
        strcpy(new_item->key, key);
        new_item->value = new jsonValue();
        p->next = new_item;
        return new_item->value;
    } else {
        jsonKeyValue *new_item = new jsonKeyValue();
        new_item->key = new char[strlen(key) + 1];
        strcpy(new_item->key, key);
        new_item->value = new jsonValue();
        first = new_item;
        return new_item->value;
    }
}

int jsonObject::toString(char *buff) {
    int i = 0;
    if (buff) { buff[i] = '{'; }
    ++i;
    jsonKeyValue *p = first;
    if (p) {
        i += p->toString(buff ? buff + i : buff);
        while (p->next) {
            if (buff) { buff[i] = ','; }
            ++i;
            p = p->next;
            i += p->toString(buff ? buff + i : buff);
        }
    }
    if (buff) { buff[i] = '}'; }
    return ++i;
}

const char *jsonObject::toString() {
    delete[]raw;
    raw = new char[toString(NULL) + 1]();
    toString(raw);
    return raw;
}




// jsonArray member   =================================================================

jsonArray::jsonArray(const char *_raw) : raw(0), first(0), error_msg(0) {
    process_raw(copy_raw(_raw, strlen(_raw)));
}

jsonArray::jsonArray(const char *_raw, int len) : raw(0), first(0), error_msg(0) {
    process_raw(copy_raw(_raw, len));
}

jsonArray::~jsonArray() {
    delete[]error_msg;
    delete[]raw;
    delete first;
}

void jsonArray::process_raw(int raw_len) {
    delete first;
    if (!raw || raw[0] != '[') {//error
        throw std::runtime_error("invalid json format");
    }
    int i = 1;
    int f;
    if ((f = find_char(raw + i, raw_len - i, ',')) == -1 && (f = find_char(raw + i, raw_len - i, ']')) == -1) {//error
        throw std::runtime_error("invalid json format");
    }
    if(f>0){
        first = new jsonValue(raw + i, f);
        jsonValue *end = first;
        i += f;
        while (i + 1 < raw_len && raw[i] == ',') {
            ++i;
            if ((f = find_char(raw + i, raw_len - i, ',')) == -1 &&
                (f = find_char(raw + i, raw_len - i, ']')) == -1) {//error
                throw std::runtime_error("invalid json format");
            }
            end->next = new jsonValue(raw + i, f);
            end = end->next;
            i += f;
        }
    }
    if (i + 1 != raw_len || raw[i] != ']') {//error
        throw std::runtime_error("invalid json format");
    }
}

const char *jsonArray::no_value_for(int index) {
    delete[]error_msg;
    int len = length();
    int index_len = num_to_char(index);
    int length_len = num_to_char(len);
    error_msg = new char[25 + index_len + length_len + 1]();
    strcpy(error_msg, "Index ");
    num_to_char(index, error_msg + 6);
    strcat(error_msg, " out of range [0..");
    num_to_char(len, error_msg + 6 + index_len + 18);
    return strcat(error_msg, ")");

//    sprintf(error_msg,"Index %d out of range [0..%d)",index,length());
//    return error_msg;
}

const char *jsonArray::value_convert_error(jsonValue *json, int index, const char *aim_type) {
    delete[]error_msg;
    size_t value_len = strlen(json->string_value);
    error_msg = new char[43 + value_len + num_to_char(index) + strlen(jsonValue::get_type_name(json->value_type)) +
                         strlen(aim_type) + 1]();

    strcpy(error_msg, "Value ");
    strcat(error_msg, json->string_value);
    strcat(error_msg, " at ");
    num_to_char(index, error_msg + 6 + value_len + 4);
    strcat(error_msg, " of type ");
    strcat(error_msg, jsonValue::get_type_name(json->value_type));
    strcat(error_msg, " cannot be converted to ");
    return strcat(error_msg, aim_type);
//    sprintf(error_msg,"Value %s at %d of type %s cannot be converted to %s",json->string_value,index,jsonValue::get_type_name(json->value_type),aim_type);
//    return error_msg;
}

const char *jsonArray::getString(int index) {
    jsonValue *item = get(index);
    if (item) {
        switch (item->value_type) {
            case jsonValue::TYPE_STRING:
            case jsonValue::TYPE_NUMBER:
            case jsonValue::TYPE_BOOL:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                return item->string_value;
            default:
                throw std::runtime_error(no_value_for(index));
        }
    }
    throw std::runtime_error(no_value_for(index));
}

double jsonArray::getDouble(int index) {
    jsonValue *item = get(index);
    if (item) {
        switch (item->value_type) {
            case jsonValue::TYPE_NUMBER:
                return item->number_value;
            case jsonValue::TYPE_STRING:
                if (check_if_num(item->string_value, strlen(item->string_value))) {
                    return get_num(item->string_value, strlen(item->string_value));
                }
            case jsonValue::TYPE_BOOL:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                throw std::runtime_error(value_convert_error(item, index, "double"));
            default:
                throw std::runtime_error(no_value_for(index));
        }
    }
    throw std::runtime_error(no_value_for(index));
}

int jsonArray::getInt(int index) {
    jsonValue *item = get(index);
    if (item) {
        switch (item->value_type) {
            case jsonValue::TYPE_NUMBER:
                return item->number_value;
            case jsonValue::TYPE_STRING:
                if (check_if_num(item->string_value, strlen(item->string_value))) {
                    return get_num(item->string_value, strlen(item->string_value));
                }
            case jsonValue::TYPE_BOOL:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                throw std::runtime_error(value_convert_error(item, index, "int"));
            default:
                throw std::runtime_error(no_value_for(index));
        }
    }
    throw std::runtime_error(no_value_for(index));
}

long jsonArray::getLong(int index) {
    jsonValue *item = get(index);
    if (item) {
        switch (item->value_type) {
            case jsonValue::TYPE_NUMBER:
                return item->number_value;
            case jsonValue::TYPE_STRING:
                if (check_if_num(item->string_value, strlen(item->string_value))) {
                    return get_num(item->string_value, strlen(item->string_value));
                }
            case jsonValue::TYPE_BOOL:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                throw std::runtime_error(value_convert_error(item, index, "long"));
            default:
                throw std::runtime_error(no_value_for(index));
        }
    }
    throw std::runtime_error(no_value_for(index));
}

bool jsonArray::getBool(int index) {
    jsonValue *item = get(index);
    if (item) {
        switch (item->value_type) {
            case jsonValue::TYPE_BOOL:
                return item->bool_value;
            case jsonValue::TYPE_STRING:
                if (strcmp(item->string_value, "true") == 0) {
                    return true;
                } else if (strcmp(item->string_value, "false") == 0) {
                    return false;
                }
            case jsonValue::TYPE_NUMBER:
            case jsonValue::TYPE_NULL:
            case jsonValue::TYPE_ARRAY:
            case jsonValue::TYPE_OBJECT:
                throw std::runtime_error(value_convert_error(item, index, "bool"));
            default:
                throw std::runtime_error(no_value_for(index));
        }
    }
    throw std::runtime_error(no_value_for(index));
}

jsonObject *jsonArray::getJsonObject(int index) {
    jsonValue *item = get(index);
    if (item) {
        if (item->value_type == jsonValue::TYPE_OBJECT) {
            return item->object;
        }
        throw std::runtime_error(value_convert_error(item, index, "jsonObject"));
    }
    throw std::runtime_error(no_value_for(index));
}

jsonArray *jsonArray::getJsonArray(int index) {
    jsonValue *item = get(index);
    if (item) {
        if (item->value_type == jsonValue::TYPE_ARRAY) {
            return item->array;
        }
        throw std::runtime_error(value_convert_error(item, index, "jsonArray"));
    }
    throw std::runtime_error(no_value_for(index));
}


jsonValue *jsonArray::get(int index) {
    jsonValue *item = first;
    while (item && index > 0) {
        --index;
        item = item->next;
    }
    if (index > 0) {
        return NULL;
    } else {
        return item;
    }
}

int jsonArray::copy_raw(const char *_raw, int len) {
    int raw_len = 0;
    bool quote = false;
    for (int i = 0; i < len; ++i) {
        if (_raw[i] == '\"') {
            quote = !quote;
        }
        if (quote || _raw[i] != ' ') {
            ++raw_len;
        }
    }
    delete[]raw;
    raw = new char[raw_len];
    int raw_index = 0;
    quote = false;
    for (int i = 0; i < len; ++i) {
        if (_raw[i] == '\"') {
            quote = !quote;
        }
        if (quote || _raw[i] != ' ') {
            raw[raw_index] = _raw[i];
            ++raw_index;
        }
    }
    return raw_len;
}

jsonArray *jsonArray::put(const char *str) {
    jsonValue *value = put();
    value->value_type = jsonValue::TYPE_STRING;
    value->string_value = new char[strlen(str) + 1];
    strcpy(value->string_value, str);
    return this;
}

jsonArray *jsonArray::put(double number) {
    jsonValue *value = put();
    value->value_type = jsonValue::TYPE_NUMBER;
    value->number_value = number;
    delete[]value->string_value;
    int len = num_to_char(number);
    value->string_value = new char[len + 1]();
    num_to_char(number, value->string_value);
    return this;
}

jsonArray *jsonArray::put(int number) {
    return put((double) number);
}

jsonArray *jsonArray::put(bool b) {
    jsonValue *value = put();
    value->value_type = jsonValue::TYPE_BOOL;
    value->bool_value = b;
    delete[]value->string_value;
    if (b) {
        value->string_value = new char[5]();
        strcpy(value->string_value, "true");
    } else {
        value->string_value = new char[6]();
        strcpy(value->string_value, "false");
    }
    return this;
}

jsonArray *jsonArray::put(jsonObject *obj) {
    jsonValue *value = put();
    value->value_type = jsonValue::TYPE_OBJECT;
    value->object = obj;
    delete[]value->string_value;
    value->string_value = new char[obj->toString(NULL) + 1]();
    obj->toString(value->string_value);
    return this;
}

jsonArray *jsonArray::put(jsonArray *arr) {
    jsonValue *value = put();
    value->value_type = jsonValue::TYPE_ARRAY;
    value->array = arr;
    delete[]value->string_value;
    value->string_value = new char[arr->toString(NULL) + 1]();
    arr->toString(value->string_value);
    return this;
}

jsonValue *jsonArray::put() {
    jsonValue *new_item = new jsonValue();
    if (first) {
        jsonValue *p = first;
        while (p->next) {
            p = p->next;
        }
        p->next = new_item;
    } else {
        first = new_item;
    }
    return new_item;
}

int jsonArray::toString(char *buff) {
    int i = 0;
    if (buff) { buff[i] = '['; }
    ++i;
    jsonValue *p = first;
    if (p) {
        i += p->toString(buff ? buff + i : buff);
        while (p->next) {
            if (buff) { buff[i] = ','; }
            ++i;
            p = p->next;
            i += p->toString(buff ? buff + i : buff);
        }
    }
    if (buff) { buff[i] = ']'; }
    ++i;
    return i;
}

const char *jsonArray::toString() {
    delete[]raw;
    raw = new char[toString(NULL) + 1]();
    toString(raw);
    return raw;
}

int jsonArray::length() {
    int length = 0;
    jsonValue *p = first;
    while (p) {
        ++length;
        p = p->next;
    }
    return length;
}
