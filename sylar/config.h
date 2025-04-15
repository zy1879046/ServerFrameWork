//
// Created by 张羽 on 25-4-11.
//

#ifndef CONFIG_H
#define CONFIG_H
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <boost/lexical_cast.hpp>
#include  <map>
#include <unordered_map>
#include <unordered_set>
#include "log.h"
#include "Singleton.h"

#define CONFIG_ENV() sylar::ConfigMgr::GetInstance()
namespace sylar
{
    //将 node 里的所有成员节点全部列到 output中
    static void ListAllMember(const std::string prefix,YAML::Node& node ,std::map<std::string,YAML::Node>& output)
    {
        //判断prefix格式是否正确
        if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos)
        {
            LOG_ROOT_ERROR() << "prefix format error: " << prefix;
            return;
        }
        output.insert(std::pair<const std::string,YAML::Node>(prefix,node));
        if (node.IsMap())
        {
            for (auto item : node)
            {
                ListAllMember(prefix.empty() ? item.first.Scalar() : prefix + "." + item.first.Scalar(),item.second,output);
            }
        }
    }

    //基础类型转换，将 From 转换为 To
    template<typename From, typename To>
    class LexicalCast
    {
    public:
        To operator()(const From& v)
        {
            return boost::lexical_cast<To>(v);
        }
    };
    //将 string 转化为 vector<T>
    template<typename T>
    class LexicalCast<std::string, std::vector<T>>
    {
    public:
        std::vector<T> operator()(const std::string& v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::vector<T> vec;
            for (auto && i : node)
            {
                std::stringstream ss;
                ss << i;
                vec.push_back(LexicalCast<std::string,T>()(ss.str()));
            }
            return vec;
        }
    };
    //将 vector<T> 转化为 string
    template<typename T>
    class LexicalCast<std::vector<T>, std::string>
    {
    public:
        std::string operator()(const std::vector<T>& v)
        {
            YAML::Node node;
            for (auto && i : v)
            {
                node.push_back(LexicalCast<T,std::string>()(i));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // 将 string 转化为 list
    template<typename T>
    class LexicalCast<std::string, std::list<T>>
    {
    public:
        std::list<T> operator()(const std::string& v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::list<T> vec;
            for (auto && i : node)
            {
                std::stringstream ss;
                ss << i;
                vec.push_back(LexicalCast<std::string,T>()(ss.str()));
            }
            return vec;
        }
    };

    //将 list<T> 转化为 string
    template<typename T>
    class LexicalCast<std::list<T>, std::string>
    {
    public:
        std::string operator()(const std::list<T>& v)
        {
            YAML::Node node;
            for (auto && i : v)
            {
                node.push_back(LexicalCast<T,std::string>()(i));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
    // 将 string 转化为 set
    template<typename T>
    class LexicalCast<std::string, std::set<T>>
    {
    public:
        std::set<T> operator()(const std::string& v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::set<T> vec;
            for (auto && i : node)
            {
                std::stringstream ss;
                ss << i;
                vec.insert(LexicalCast<std::string,T>()(ss.str()));
            }
            return vec;
        }
    };
    //将 set<T> 转化为 string
    template<typename T>
    class LexicalCast<std::set<T>, std::string>
    {
    public:
        std::string operator()(const std::set<T>& v)
        {
            YAML::Node node;
            for (auto && i : v)
            {
                node.push_back(LexicalCast<T,std::string>()(i));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    //将 string 转化为 unordered_set
    template<typename T>
    class LexicalCast<std::string, std::unordered_set<T>>
    {
    public:
        std::unordered_set<T> operator()(const std::string& v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::unordered_set<T> vec;
            for (auto && i : node)
            {
                std::stringstream ss;
                ss << i;
                vec.insert(LexicalCast<std::string,T>()(ss.str()));
            }
            return vec;
        }
    };
    //将 unordered_set<T> 转化为 string
    template<typename T>
    class LexicalCast<std::unordered_set<T>, std::string>
    {
    public:
        std::string operator()(const std::unordered_set<T>& v)
        {
            YAML::Node node;
            for (auto && i : v)
            {
                node.push_back(LexicalCast<T,std::string>()(i));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
    //将 string 转化为 map
    template<typename T>
    class LexicalCast<std::string, std::map<std::string, T>>
    {
    public:
        std::map<std::string, T> operator()(const std::string& v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::map<std::string, T> vec;
            for (auto && i : node)
            {
                std::stringstream ss;
                ss << i.second;
                vec.insert(std::make_pair(i.first.as<std::string>(), LexicalCast<std::string,T>()(ss.str())));
            }
            return vec;
        }
    };
    //将 map<T> 转化为 string
    template<typename T>
    class LexicalCast<std::map<std::string, T>, std::string>
    {
    public:
        std::string operator()(const std::map<std::string, T>& v)
        {
            YAML::Node node;
            for (auto && i : v)
            {
                node[i.first] = LexicalCast<T,std::string>()(i.second);
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
    //将 string 转化为 unordered_map
    template<typename T>
    class LexicalCast<std::string, std::unordered_map<std::string, T>>
    {
    public:
        std::unordered_map<std::string, T> operator()(const std::string& v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::unordered_map<std::string, T> vec;
            for (auto && i : node)
            {
                std::stringstream ss;
                ss << i.second;
                vec.insert(std::make_pair(i.first.as<std::string>(), LexicalCast<std::string,T>()(ss.str())));
            }
            return vec;
        }
    };
    //将 unordered_map<T> 转化为 string
    template<typename T>
    class LexicalCast<std::unordered_map<std::string, T>, std::string>
    {
    public:
        std::string operator()(const std::unordered_map<std::string, T>& v)
        {
            YAML::Node node;
            for (auto && i : v)
            {
                node[i.first] = LexicalCast<T,std::string>()(i.second);
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

//定义一个配置变量基类 变量的名字为 xx.xxx.xxx
    class ConfigVarBase
    {
    public:
        using ptr = std::shared_ptr<ConfigVarBase>;
        explicit ConfigVarBase(const std::string& name,
                      const std::string& description = "")
            :m_name(name)
            ,m_description(description)
        {
            //变量名字大小写不敏感
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
        }
        virtual ~ConfigVarBase() = default;
        //将当前变量的值转化为 string
        virtual std::string to_string() const = 0;
        //将 string 转化为当前变量的值
        virtual bool from_string(const std::string& v) = 0;
    private:
        std::string m_name;//配置变量名称
        std::string m_description;//描述
    };
    //配置变量类
    template<typename T,typename FromStr = LexicalCast<std::string,T>,typename ToStr = LexicalCast<T,std::string>>
    class ConfigVar : public ConfigVarBase
    {
    public:
        using ptr = std::shared_ptr<ConfigVar>;
        // old_value -> new_value 需要调用的回调函数
        using on_changed_callback = std::function<void(const T&, const T&)>;
        ConfigVar(const std::string& name,const T& default_value,
                  const std::string& description = "")
            :ConfigVarBase(name,description)
            ,m_value(default_value)
        {
        }
        //将当前变量的值转化为 string
        std::string to_string() const override
        {
            try
            {
                return ToStr()(m_value);
            }
            catch (std::exception& e)
            {
                LOG_ROOT_INFO() << "ConfigVar::to_string exception: " << e.what() << std::endl;
                return "";
            }
        }
        //将 string 转化为当前变量的值
        bool from_string(const std::string& v) override
        {
            try
            {
                set_value(FromStr()(v));
                return true;
            }
            catch (std::exception& e)
            {
                LOG_ROOT_INFO() << "ConfigVar::from_string exception: " << e.what() << std::endl;
                return false;
            }
        }

        const T& get_value() const
        {
            return m_value;
        }
        void set_value(const T& v)
        {
            if (m_value == v)
            {
                return;
            }
            for (auto& i : m_callbacks)
            {
                i.second(m_value, v);
            }
            m_value = v;
        }

        uint64_t add_listener(on_changed_callback cb)
        {
            static uint64_t key = 0;
            ++key;
            m_callbacks[key] = cb;
            return key;
        }

        void del_listener(uint64_t key)
        {
            auto it = m_callbacks.find(key);
            if (it != m_callbacks.end())
            {
                m_callbacks.erase(it);
            }
        }
        void clear_listener()
        {
            m_callbacks.clear();
        }
    private:
        T m_value;//配置变量值
        std::unordered_map<uint64_t,on_changed_callback> m_callbacks;
    };
    //配置类
    class ConfigMgr : public Singleton<ConfigMgr>
    {
    public:
        using ConfigVarMap = std::map<std::string,ConfigVarBase::ptr>;
        ConfigMgr() = default;
        ~ConfigMgr() = default;
        //查找配置变量是否存在
        ConfigVarBase::ptr lookup_base(const std::string& name) const
        {
            auto iter = m_config.find(name);
            return iter != m_config.end() ? iter->second : nullptr;
        }
        void load_from_yaml(YAML::Node& node) const
        {
            std::map<std::string,YAML::Node> output;
            ListAllMember("",node,output);
            //将节点中把我们需要的配置读取出来
            for (auto& item : output)
            {
                //判断当前配置是否是需要的配置
                std::string name = item.first;
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                //从配置变量中是否有该配置
                auto var = lookup_base(name);
                if (var != nullptr)
                {
                    if (item.second.IsScalar())
                    {
                        var->from_string(item.second.Scalar());
                    }else
                    {
                        std::stringstream ss;
                        ss << item.second;
                        var->from_string(ss.str());
                    }
                }
            }
        }
        //查找或创建配置
        template<typename T>
        typename ConfigVar<T>::ptr lookup(const std::string& name, const T& default_value,
                                          const std::string& description = "")
        {
            auto iter = m_config.find(name);
            //配置存在则进行转换返回
            if (iter != m_config.end())
            {
                auto var = std::dynamic_pointer_cast<ConfigVar<T>>(iter->second);
                if (var)
                {
                    return var;
                }
                else
                {
                    LOG_ROOT_ERROR() << "ConfigVar name: " << name << " is not the same type";
                    return nullptr;
                }
            }
            else//不存在则创建
            {
                auto var = std::make_shared<ConfigVar<T>>(name,default_value,description);
                m_config[name] = var;
                return var;
            }
        }
        template<typename T>
        typename ConfigVar<T>::ptr lookup(const std::string& name)
        {
            auto iter = m_config.find(name);
            //配置存在则进行转换返回
            if (iter != m_config.end())
            {
                auto var = std::dynamic_pointer_cast<ConfigVar<T>>(iter->second);
                if (var)
                {
                    return var;
                }
                else
                {
                    LOG_ROOT_ERROR() << "ConfigVar name: " << name << " is not the same type";
                    return nullptr;
                }
            }
            else//不存在则返回 nullptr
            {
                LOG_ROOT_ERROR() << "ConfigVar name: " << name << " is not exist";
                return nullptr;
            }
        }
    // 查看当前配置
        void visit(std::function<void(const ConfigVarBase::ptr&)> cb)
        {
            for (auto& i : m_config)
            {
                cb(i.second);
            }
        }
    private:
        ConfigVarMap m_config;
    };
}



#endif //CONFIG_H
