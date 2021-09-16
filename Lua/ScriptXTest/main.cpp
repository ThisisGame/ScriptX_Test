#include <iostream>

#include <ScriptX/ScriptX.h>

using namespace script;

std::shared_ptr<script::ScriptEngine> createEngine() {
#if !defined(SCRIPTX_BACKEND_WEBASSEMBLY)
    return std::shared_ptr<script::ScriptEngine>{new script::ScriptEngineImpl(),
                                                 script::ScriptEngine::Deleter()};
#else
    return std::shared_ptr<script::ScriptEngine>{script::ScriptEngineImpl::instance(), [](void*) {}};
#endif
}

class MyImage : public script::ScriptClass {
public:
    using script::ScriptClass::ScriptClass;

    MyImage() : script::ScriptClass(script::ScriptClass::ConstructFromCpp<MyImage>{}) {}

    static const int age = 2;
    static const char* name() { return "name is MyImage"; }

    std::string src;
    int add(int a, int b) {return a+ b;}
};
const int MyImage::age;

class BaseClass {
public:
    int age = 0;
    int num = 1;
    const int length = 180;

    std::string name() { return "name is BaseClass"; }

    int getNum() { return num; }

    void setNum(int n) { num = n; }
};

class BaseClassScriptWrapper : public BaseClass, public script::ScriptClass {
public:
    explicit BaseClassScriptWrapper(const script::Local<script::Object>& thiz) : BaseClass(), ScriptClass(thiz) {}
};


int main() {
    auto engine = createEngine();
    script::EngineScope enter(engine.get());

    auto luaEng = EngineScope::currentEngineAs<lua_backend::LuaEngine>();
    auto lua = lua_interop::getEngineLua(luaEng);

    engine->eval("print('hello world')");

    script::ClassDefine<MyImage> myClassDefine =
            script::defineClass<MyImage>("MyImage")
                    .constructor()
                    .property("age", &MyImage::age)
                    .function("name", &MyImage::name)
                    .instanceFunction("add", &MyImage::add)
                    .instanceProperty("src", &MyImage::src)
                    .build();
    engine->registerNativeClass<MyImage>(myClassDefine);
    engine->eval("print(MyImage.name())");

    const auto baseWrapperDefine =
            script::defineClass<BaseClassScriptWrapper>("BaseClass")
                    .nameSpace("TestNameSpace")
                    .constructor()
                    .instanceProperty("age", &BaseClass::age)
                    .instanceProperty("length", &BaseClass::length)  // const property has getter, no setter
                    .instanceFunction("name", &BaseClass::name)
                    .instanceProperty("num", &BaseClass::getNum, &BaseClass::setNum)
                    .build();
    engine->registerNativeClass<BaseClassScriptWrapper>(baseWrapperDefine);
    engine->eval("print(\"BaseClass:\",TestNameSpace.BaseClass)");
    engine->eval("print(TestNameSpace.BaseClass():name())");

    //lua扩展c++ class
    std::cout<<"-------- lua extend cpp class----------"<<std::endl;
    {
        engine->eval(R"(
            print("lua extend cpp class TestNameSpace.BaseClass")

            local meta = ScriptX.getInstanceMeta(TestNameSpace.BaseClass)
            function meta:__add()
                print(self, "add")
            end

            function meta.instanceFunction:hello()
                print(self, "hello")
            end

            local base_class=TestNameSpace.BaseClass()
            local x=base_class+base_class
            base_class:hello()
        )");

        //获取扩展后的 BaseClass
        std::cout<<"-------- cpp get lua extend class----------"<<std::endl;
        {
            engine->eval("return TestNameSpace.BaseClass");
            auto handle_base_class=lua_gettop(lua);
            std::cout<<lua_type(lua,handle_base_class)<<" "<<lua_typename(lua,lua_type(lua,handle_base_class))<<std::endl;
            auto handle_base_class_object = lua_interop::makeLocal<Object>(handle_base_class);

            //获取BaseClass()
            lua_pushstring(lua,"__call");
            lua_gettable(lua,-2);
            auto f=lua_gettop(lua);
            std::cout<<lua_type(lua,f)<<" "<<lua_typename(lua,lua_type(lua,f))<<std::endl;

            if(lua_isfunction(lua,f)){

            }
        }
    }


    //交互
    std::cout<<"-------- lua interacts with cpp ----------"<<std::endl;
    {
        auto obj = Object::newObject();
        auto jobj = lua_interop::toLua<Object>(obj);
        std::cout<<"lua_istable(lua, jobj):"<<lua_istable(lua, jobj)<<std::endl;

        lua_pushnumber(lua, 3.14);
        auto lnum = lua_gettop(lua);
        auto num = lua_interop::makeLocal<Number>(lnum);
        std::cout<<"num.toDouble()==3.14:"<<(num.toDouble()==3.14)<<std::endl;

        lua_pushstring(lua, "hello world");
        auto lstr = lua_gettop(lua);
        auto str = lua_interop::makeLocal<String>(lstr);
        std::cout<<str.toString()<<std::endl;
        std::cout<<"(str.toString().c_str() == \"hello world\"):"<<(str.toString().c_str() == "hello world")<<std::endl;

        auto func = Function::newFunction([](const Arguments& args) -> Local<Value> {
            auto data = lua_interop::extractArguments(args);
            auto lua = lua_interop::getEngineLua(data.engine);
            auto a = lua_tonumber(lua, data.stackBase);
            auto b = lua_tonumber(lua, data.stackBase + 1);

            lua_pushnumber(lua, a + b);
            auto ret = lua_gettop(lua);

            return lua_interop::makeLocal<Number>(ret);
        });
        auto func_ret = func.call({}, 1, 2);
        std::cout<<"func_ret.isNumber():"<<func_ret.isNumber()<<std::endl;
        std::cout<<"func_ret:"<<func_ret.asNumber().toDouble()<<std::endl;

        //c++调用lua函数
        std::cout<<"-------- cpp call lua function ----------"<<std::endl;
        {
            engine->eval("function compare(a,b) print(\"a==b:\" .. tostring(a==b)) end");
            lua_getglobal(lua,"compare");
            auto v=lua_gettop(lua);

            std::cout<<lua_type(lua,v)<<" "<<lua_typename(lua,lua_type(lua,v))<<std::endl;

            if(lua_isfunction(lua,v)){
                auto func_compare=lua_interop::makeLocal<Function>(v);
                func_compare.call({},1,2);
            }
        }

        //c++调用lua table函数
        std::cout<<"-------- cpp call lua table function ----------"<<std::endl;
        {
            engine->eval("Player={} function Player:Compare(a,b) print(tostring(self),tostring(a),tostring(b)) print(\"a==b:\" .. tostring(a==b)) end");
            //获取Player
            lua_getglobal(lua,"Player");
            auto handle_player=lua_gettop(lua);
            std::cout<<lua_type(lua,handle_player)<<" "<<lua_typename(lua,lua_type(lua,handle_player))<<std::endl;

            //检测cpp两次获取lua同一个table，cpp端临时变量是否相等
            if(lua_istable(lua,handle_player)){
                std::cout<<"-------- check cpp ref lua table equal ----------"<<std::endl;
                {
                    auto object1=lua_interop::makeLocal<Object>(handle_player);
                    auto object2=lua_interop::makeLocal<Object>(handle_player);
                    std::cout<<"object1==object2:"<<(object1==object2)<<std::endl;
                }
            }

            //获取Player.Compare
            lua_pushstring(lua,"Compare");
            lua_gettable(lua,-2);
            auto f=lua_gettop(lua);
            std::cout<<lua_type(lua,f)<<" "<<lua_typename(lua,lua_type(lua,f))<<std::endl;

            if(lua_isfunction(lua,f)){
                //检测cpp两次获取lua同一个table，cpp端临时变量是否相等
                std::cout<<"-------- check lua ref cpp instance equal ----------"<<std::endl;
                {
                    auto cpp_instance=new MyImage();
                    auto func_compare=lua_interop::makeLocal<Function>(f);
                    func_compare.call(lua_interop::makeLocal<Object>(handle_player),cpp_instance,cpp_instance);
                }
            }
        }

        //在c++代码里比较lua返回对象实例
        std::cout<<"-------- cpp compare lua return instance ----------"<<std::endl;
        {
            engine->eval("function set_and_get(a) print(a) print(\"a is instanceof(MyImage): \" .. tostring(ScriptX.isInstanceOf(a,MyImage))) return a end");
            //获取函数 set_game_object
            lua_getglobal(lua,"set_and_get");
            auto handle_function=lua_gettop(lua);
            std::cout<<lua_type(lua,handle_function)<<" "<<lua_typename(lua,lua_type(lua,handle_function))<<std::endl;

            if(lua_isfunction(lua,handle_function)){
                auto cpp_instance=new MyImage();
                auto func_local=lua_interop::makeLocal<Function>(handle_function);
                auto return_local_value=func_local.call({},cpp_instance);
                if(engine->isInstanceOf<MyImage>(return_local_value)){
                    std::cout<<"cpp_instance->getScriptObject()==return_local_value: "<<(cpp_instance->getScriptObject()==return_local_value)<<std::endl;
                    std::cout<<"cpp_instance==engine->getNativeInstance<MyImage>(return_local_value): "<<(cpp_instance==engine->getNativeInstance<MyImage>(return_local_value))<<std::endl;
                }
            }
        }
    }




    return 0;
}
