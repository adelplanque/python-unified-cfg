#include <arpa/inet.h>
#include <iomanip>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <string>

#include <cfg/config.h>
#include <cfg/render.h>
#include <cfg/settings.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace cfg;

settings_t::ptr settings = std::make_shared<settings_t>();


class SettingsIterator
{
public:
    SettingsIterator(settings_t::ptr settings)
        : settings(settings)
        , it(settings->begin())
        , end(settings->end())
    {}

protected:
    settings_t::ptr settings;
    settings_t::iterator it;
    settings_t::iterator end;
};


class SettingsKeyIterator : public SettingsIterator
{
public:
    SettingsKeyIterator(settings_t::ptr settings) : SettingsIterator(settings)
    {}

    const std::string& next()
    {
        try {
            if (this->it.value() == this->end.value()) {
                throw pybind11::stop_iteration();
            } else {
                return (this->it.value()++)->first;
            }
        }
        catch (std::bad_optional_access&) {
            throw pybind11::stop_iteration();
        }
    }
};


class SettingsValueIterator : public SettingsIterator
{
public:
    SettingsValueIterator(settings_t::ptr settings) : SettingsIterator(settings)
    {}

    py::object next()
    {
        try {
            if (this->it.value() == this->end.value()) {
                throw pybind11::stop_iteration();
            } else {
                auto& value = this->it.value()->second;
                ++this->it.value();
                if (value->is_value()) {
                    return py::str(value->as<std::string>());
                } else {
                    return py::cast(value);
                }
            }
        }
        catch (std::bad_optional_access&) {
            throw pybind11::stop_iteration();
        }
    }
};


class SettingsItemIterator : public SettingsIterator
{
public:
    SettingsItemIterator(settings_t::ptr settings) : SettingsIterator(settings)
    {}

    py::object next()
    {
        try {
            if (this->it.value() == this->end.value()) {
                throw pybind11::stop_iteration();
            } else {
                auto& key = this->it.value()->first;
                auto& value = this->it.value()->second;
                ++this->it.value();
                if (value->is_value()) {
                    return py::make_tuple(key, value->as<std::string>());
                } else {
                    return py::make_tuple(key, value);
                }
            }
        }
        catch (std::bad_optional_access&) {
            throw pybind11::stop_iteration();
        }
    }
};


template<typename T>
class Cast
{
public:
    Cast(settings_t::ptr settings) : settings(settings) {}

    py::object call(const std::string& key, bool no_raise, py::object& failback)
    {
        try {
            return py::int_(settings->at(key)->as<T>());
        }
        catch (const std::out_of_range& e) {
            if (no_raise) return failback;
            else throw e;
        }
        catch (boost::bad_lexical_cast&) {
            std::ostringstream oss;
            oss << std::quoted(settings->at(key)->as<std::string>());
            throw pybind11::value_error(oss.str());
        }
    }

private:
    settings_t::ptr settings;
};


/**
 * Retourne l'adresse IP sous la forme "X.Y.Z.T" du hostname passé en argument
 *
 * La résolution effectuée est standard via getaddrinfo()
 *
 * @return chaine allouée (à libérer par g_free()) contenant l'adresse IP (NULL si pas trouvé)
 */
std::string get_ip_for_hostname(const std::string& hostname)
{
    struct addrinfo *info, *p;
    std::string res;
    if ((hostname == "127.0.0.1") || (hostname == "localhost")
        || (hostname == "localhost.localdomain")) {
        res = "127.0.0.1";
    } else if ((getaddrinfo(hostname.c_str(), NULL, NULL, &info)) == 0) {
        p = info;
        while (p != NULL) {
            if (p->ai_addr != NULL && p->ai_family == AF_INET) {
                struct sockaddr_in *sock_in = (struct sockaddr_in *) p->ai_addr;
                const char *tmp = inet_ntoa(sock_in->sin_addr);
                if (tmp != NULL) {
                    if (strcmp(tmp, "127.0.0.1") != 0) {
                        res = tmp;
                        break;
                    } else {
                        /*
                         * si c'est 127.0.0.1, on prend mais on va essayer de trouver autre chose
                         */
                        if (res.empty()) {
                            res = tmp;
                        }
                    }
                }
            }
            p = p->ai_next;
        }
        freeaddrinfo(info);
    }
    return res;
}


class IpResolver
{
public:
    IpResolver(settings_t::ptr settings) : settings(settings) {}

    std::string& call(const std::string& key)
    {
        auto hostname = settings->at(key)->as<std::string>();
        if (! IpResolver::cache.count(hostname)) {
            IpResolver::cache.emplace(hostname, get_ip_for_hostname(hostname));
        }
        return IpResolver::cache.at(hostname);
    }

private:
    settings_t::ptr settings;
    static std::map<std::string, std::string> cache;
};

std::map<std::string, std::string> IpResolver::cache = std::map<std::string, std::string>();


void set_config_path(py::object paths)
{
    config_t& cfg = config_t::get_instance();
    cfg.clear_config_path();
    settings->clear();
    if (py::isinstance<py::str>(paths)) {
        std::string p = paths.cast<std::string>();
        cfg.push_back_config_path(p);
    } else {
        auto it = py::iter(paths);
        while (it != py::iterator::sentinel()) {
            std::string p = it->cast<std::string>();
            cfg.push_back_config_path(p);
            ++it;
        }
    }
};


std::list<std::string> get_config_path()
{
    std::list<std::string> res;
    for (const auto& path : config_t::get_instance().get_config_path()) {
        res.push_back(path.c_str());
    }
    return res;
};


PYBIND11_MODULE(_cfg, m) {
    m.doc() = "";

    py::class_<Cast<long>>(m, "_cast_int")
        .def("__call__", &Cast<long>::call,
             py::arg("key"), py::arg("no_raise") = false, py::arg("failback") = py::none());

    py::class_<Cast<bool>>(m, "_cast_bool")
        .def("__call__", &Cast<bool>::call,
             py::arg("key"), py::arg("no_raise") = false, py::arg("failback") = py::none());

    py::class_<IpResolver>(m, "_cast_ip")
        .def("__call__", &IpResolver::call, py::arg("key"));

    py::class_<SettingsKeyIterator, std::unique_ptr<SettingsKeyIterator>>
        (m, "_settings_keys")
        .def("__iter__", [](py::object self) { return self; })
        .def("__next__", &SettingsKeyIterator::next);

    py::class_<SettingsValueIterator, std::unique_ptr<SettingsValueIterator>>
        (m, "_settings_values")
        .def("__iter__", [](py::object self) { return self; })
        .def("__next__", &SettingsValueIterator::next);

    py::class_<SettingsItemIterator, std::unique_ptr<SettingsItemIterator>>
        (m, "_settings_items")
        .def("__iter__", [](py::object self) { return self; })
        .def("__next__", &SettingsItemIterator::next);

    py::class_<settings_t, settings_t::ptr>(m, "_settings")
        .def("__getattr__", [](settings_t& self, const std::string key) {
            py::object o;
            try {
                auto value = self.at(key);
                if (value->is_value()) {
                    o = py::str(value->as<std::string>());
                } else {
                    o = py::cast(value);
                }
            }
            catch (const std::out_of_range& e) {
                throw py::attribute_error(e.what());
            }
            return o;
        }, py::arg("key"))
        .def("__contains__", [](settings_t& self, const std::string& key) {
            return self.count(key) > 0;
        }, py::arg("key"))
        .def("__iter__", [](settings_t& self) {
            return std::make_unique<SettingsKeyIterator>(self.shared_from_this());
        })
        .def("keys", [](settings_t& self) {
            return std::make_unique<SettingsKeyIterator>(self.shared_from_this());
        })
        .def("values", [](settings_t& self) {
            return std::make_unique<SettingsValueIterator>(self.shared_from_this());
        })
        .def("items", [](settings_t& self) {
            return std::make_unique<SettingsItemIterator>(self.shared_from_this());
        })
        .def("get",
             [](settings_t::ptr& self, const std::string key, py::object def) {
                 py::object o;
                 try {
                     auto value = self->at(key);
                     if (value->is_value()) {
                         o = py::str(value->as<std::string>());
                     } else {
                         o = py::cast(value);
                     }
                 }
                 catch (const std::out_of_range& e) {
                     o = def;
                 }
                 return o;
             },
             py::arg("key"), py::arg("default") = py::none())
        .def_property_readonly("as_int", [](settings_t& self) {
                return Cast<long>(self.shared_from_this());
        })
        .def_property_readonly("as_bool", [](settings_t& self) {
            return Cast<bool>(self.shared_from_this());
        })
        .def_property_readonly("as_ip", [](settings_t& self) {
            return IpResolver(self.shared_from_this());
        });

    m.def("set_config_path", &set_config_path);
    m.def("get_config_path", &get_config_path);
    m.def("render", [](const std::string& txt) {
        auto is = std::istringstream(txt);
        return render(settings, is);
    }, "Render template file");

    m.attr("settings") = settings;

}
