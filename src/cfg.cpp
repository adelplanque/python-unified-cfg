#include <iomanip>
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

    settings_t::ptr next()
    {
        try {
            if (this->it.value() == this->end.value()) {
                throw pybind11::stop_iteration();
            } else {
                return (this->it.value()++)->second;
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

    std::pair<const std::string&, settings_t::ptr> next()
    {
        try {
            if (this->it.value() == this->end.value()) {
                throw pybind11::stop_iteration();
            } else {
                return *(this->it.value()++);
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

    T call(const std::string& key)
    {
        try {
            return settings->at(key)->as<T>();
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

    py::class_<Cast<int>>(m, "_cast_int")
        .def("__call__", &Cast<int>::call, py::arg("key"));

    py::class_<Cast<bool>>(m, "_cast_bool")
        .def("__call__", &Cast<bool>::call, py::arg("key"));

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
        .def("__getattr__",
             [](settings_t::ptr& self, const std::string key) {
                 auto value = self->at(key);
                 py::object o;
                 if (value->is_value()) {
                     o = py::str(value->as<std::string>());
                 } else {
                     o = py::cast(value);
                 }
                 return o;
             },
             py::arg("key"))
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
        .def_property_readonly("as_int", [](settings_t& self) {
            return Cast<int>(self.shared_from_this());
        })
        .def_property_readonly("as_bool", [](settings_t& self) {
            return Cast<bool>(self.shared_from_this());
        });

    m.def("set_config_path", &set_config_path);
    m.def("get_config_path", &get_config_path);
    m.def("render", [](const std::string& txt) {
        auto is = std::istringstream(txt);
        return render(settings, is);
    }, "Render template file");

    m.attr("settings") = settings;

}
