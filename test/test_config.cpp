//
// Created by 张羽 on 25-4-12.
//

#include "config.h"
#include "log.h"

class Person{
public:
    std::string m_name;
    int m_age = 0;
    bool m_sex = 0;
    const std::string toString()const{
        std::stringstream ss;
        ss << "[Person name = " << m_name
            <<" age = "<< m_age
            <<" sex = " << m_sex
            <<"]";
        return ss.str();
    }
    bool operator==(const Person& other)const{
        return m_name == other.m_name &&
            m_age == other.m_age &&
            m_sex == other.m_sex;
    }
};
namespace sylar{
    template <>
    class LexicalCast<std::string,Person>{
    public: Person operator()(const std::string& v){
        YAML::Node node = YAML::Load(v);
        Person p;
        p.m_name = node["name"].as<std::string>();
        p.m_age = node["age"].as<int>();
        p.m_sex = node["sex"].as<bool>();
        return p;
    }
    };

    template<>
    class LexicalCast<Person,std::string>{
    public:
        std::string operator()(const Person& p){
            YAML::Node node;
            node["name"] = p.m_name;
            node["age"] = p.m_age;
            node["sex"] = p.m_sex;
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
};

sylar::ConfigVar<int>::ptr g_int_value_config = CONFIG_ENV()->lookup("system.port",(int)8080,"system port");
//sylar::ConfigVar<float>::ptr g_int_value_config_2 = CONFIGENV()->lookup("system.port",(float)8080,"system port");
sylar::ConfigVar<float>::ptr g_float_value_config = CONFIG_ENV()->lookup("system.value" ,(float)10.2f,"system value");
sylar::ConfigVar<std::vector<int>>::ptr g_vec_value_config = CONFIG_ENV()->lookup("system.vec",std::vector<int>{1,2,3},"system vec");
sylar::ConfigVar<std::list<int>>::ptr g_list_value_config = CONFIG_ENV()->lookup("system.lst",std::list<int>{3,4,5,6},"system list");
sylar::ConfigVar<std::set<int>>::ptr g_set_value_config = CONFIG_ENV()->lookup("system.set",std::set<int>{2,5,7},"system set");
sylar::ConfigVar<std::map<std::string,int>>::ptr g_map_value_config = CONFIG_ENV()->lookup("system.map",std::map<std::string,int>{{"aa",1}},"system map");
sylar::ConfigVar<Person>::ptr g_Person_value_config = CONFIG_ENV()->lookup("class.person",Person(),"system person");


void print_yaml(const YAML::Node& node,int level){
    if(node.IsScalar()){
        LOG_ROOT_DEBUG() << std::string(level * 4,' ') << node.Scalar() <<" - " << node.Type() <<" - " << level;
    }else if(node.IsNull()){
        LOG_ROOT_DEBUG() << std::string(level * 4 ,' ') <<"NULL - " <<node.Type() <<" - "<<level;
    }else if(node.IsMap()){
        for(auto it = node.begin(); it != node.end();++it){
            LOG_ROOT_DEBUG() << std::string(level * 4,' ')<<it->first <<" - " <<it->second.Type() <<" - " <<level;
            print_yaml(it->second,level + 1);
        }
    }else if(node.IsSequence()){
        for(size_t i = 0; i < node.size(); ++i){
            LOG_ROOT_DEBUG() << std::string(level *4 ,' ')<< i << " - "<< node[i].Type() << " - " <<level;
            print_yaml(node[i],level + 1);
        }
    }
}
void test_yaml(){
    YAML::Node root = YAML::LoadFile("../bin/config/log.yml");
    print_yaml(root,0);
}
void test_config(){
    //LOG_ROOT_INFO << "before: " << g_int_value_config->getValue();
    // LOG_ROOT_INFO << "before: " <<g_int_value_config_2->getValue();
#define XX(g_var,name,prefix) \
    { \
        auto v = g_var->getValue(); \
        auto res = g_var->toString(); \
        std::cout << res << std::endl; \
        for(auto& i : v){ \
            LOG_ROOT_INFO << #prefix " " #name ": " <<i ; \
        } \
    }

#define XX_M(g_var,name,prefix) \
    { \
        auto v = g_var->getValue(); \
        for(auto& i : v) \
        { \
            LOG_ROOT_INFO() << #prefix " " #name ": {" << i.first <<" : " << i.second <<"}"; \
        } \
    }
    g_Person_value_config->add_listener([](const Person& old_value, const Person& new_value){
            LOG_ROOT_INFO()<<"old_value:  " << old_value.toString()
                <<"newValue: "<< new_value.toString();
            });
    LOG_ROOT_INFO()<< "before: "<< g_Person_value_config->get_value().toString();
    LOG_ROOT_INFO() << "before: " << g_Person_value_config->to_string();
    //XX(g_Person_value_config,person,before);
    // XX(g_list_value_config,lst,before);
    // XX(g_set_value_config,set,before);
    // XX_M(g_map_value_config,map,before);
    YAML::Node root = YAML::LoadFile("../bin/config/log.yml");
    CONFIG_ENV()->load_from_yaml(root);

    LOG_ROOT_INFO()<< "after: "<< g_Person_value_config->get_value().toString();
    //XX(g_Person_value_config,person,before);
    //XX(g_Person_value_config,person,after);
    // XX(g_list_value_config,lst,after);
    // XX(g_set_value_config,set,after);
    // XX_M(g_map_value_config,map,after);
}

void test_log(){
    static sylar::Logger::ptr system_log = LOG_NAME("system");
    LOG_INFO(system_log) << "hello system" <<std::endl;
    std::cout << LOG_MGR()->to_yaml_string() << std::endl;
    // std::cout << sylar::g_log_defines->toString() <<std::endl;
    YAML::Node root = YAML::LoadFile("../bin/config/log.yml");
    CONFIG_ENV()->load_from_yaml(root);
    std::cout << "============================" << std::endl;
    //std::cout << sylar::g_log_defines->toString() << std::endl;
    std::cout << LOG_MGR()->to_yaml_string() << std::endl;
    LOG_INFO(system_log) << "hello system";
    system_log->set_formatter("%d-  %m%n");
    LOG_INFO(system_log) << "hello system";
    //std::cout << sylar::LoggerMgr::GetInstance()->toYamlString() << std::endl;
}
int main()
{
    // test_yaml();
    // test_config();
    test_log();
    return 0;


}