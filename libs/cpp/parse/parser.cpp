#include "parser.h"

#include <cinttypes>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "nlohmann_json/include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

#ifndef LIBEVENTS_PARSER_DEBUG_PRINTF
#define LIBEVENTS_PARSER_DEBUG_PRINTF(...)
#endif

string& ltrim(std::string& str)
{
    auto it2 =
        std::find_if(str.begin(), str.end(), [](char ch) { return !std::isspace<char>(ch, std::locale::classic()); });
    str.erase(str.begin(), it2);
    return str;
}

string& rtrim(std::string& str)
{
    auto it1 =
        std::find_if(str.rbegin(), str.rend(), [](char ch) { return !std::isspace<char>(ch, std::locale::classic()); });
    str.erase(it1.base(), str.end());
    return str;
}

string& trim(std::string& str)
{
    return ltrim(rtrim(str));
}

namespace events
{
namespace parser
{
BaseType fromString(const std::string& base_type)
{
    if (base_type == "uint8_t") {
        return BaseType::uint8_t;
    } else if (base_type == "int8_t") {
        return BaseType::int8_t;
    } else if (base_type == "uint16_t") {
        return BaseType::uint16_t;
    } else if (base_type == "int16_t") {
        return BaseType::int16_t;
    } else if (base_type == "uint32_t") {
        return BaseType::uint32_t;
    } else if (base_type == "int32_t") {
        return BaseType::int32_t;
    } else if (base_type == "uint64_t") {
        return BaseType::uint64_t;
    } else if (base_type == "int64_t") {
        return BaseType::int64_t;
    } else if (base_type == "float") {
        return BaseType::float_t;
    }
    return BaseType::invalid;
}

int baseTypeSize(BaseType base_type)
{
    switch (base_type) {
        case BaseType::uint8_t: return 1;
        case BaseType::int8_t: return 1;
        case BaseType::uint16_t: return 2;
        case BaseType::int16_t: return 2;
        case BaseType::uint32_t: return 4;
        case BaseType::int32_t: return 4;
        case BaseType::uint64_t: return 8;
        case BaseType::int64_t: return 8;
        case BaseType::float_t: return 4;
        case BaseType::invalid: break;
    }
    return 0;
}

ParsedEvent::ParsedEvent(const EventType& event, const Config& config, const EventDefinition& event_definition,
                         const EnumDefinitions& enums)
    : _event(event), _config(config), _event_definition(event_definition), _enums(enums)
{
}

string ParsedEvent::message() const
{
    return processMessage(_event_definition.message);
}

string ParsedEvent::description() const
{
    return processMessage(_event_definition.description);
}

size_t ParsedEvent::find(const std::string& s, const string& search_chars, size_t start_pos)
{
    for (size_t i = start_pos; i < s.size(); ++i) {
        // ignore escaped chars
        if (s[i] == '\\') {
            ++i;
            continue;
        }

        if (search_chars.find(s[i]) != string::npos) {
            return i;
        }
    }

    return string::npos;
}

size_t ParsedEvent::findClosingTag(const string& s, size_t start_pos, const string& tag)
{
    string search = "</" + tag + ">";
    return s.find(search, start_pos);
}

string ParsedEvent::getFormattedArgument(int argument_idx, int num_decimal_digits, const std::string& unit) const
{
    std::ostringstream argument_stream;
    Argument arg = argumentValue(argument_idx);
    uint64_t value = 0;

    switch (_event_definition.arguments[argument_idx].type) {
        case BaseType::uint8_t:
            value = arg.value.val_uint8_t;
            argument_stream << (int)arg.value.val_uint8_t;
            break;
        case BaseType::int8_t:
            value = arg.value.val_int8_t;
            argument_stream << (int)arg.value.val_int8_t;
            break;
        case BaseType::uint16_t:
            value = arg.value.val_uint16_t;
            argument_stream << arg.value.val_uint16_t;
            break;
        case BaseType::int16_t:
            value = arg.value.val_int16_t;
            argument_stream << arg.value.val_int16_t;
            break;
        case BaseType::uint32_t:
            value = arg.value.val_uint32_t;
            argument_stream << arg.value.val_uint32_t;
            break;
        case BaseType::int32_t:
            value = arg.value.val_int32_t;
            argument_stream << arg.value.val_int32_t;
            break;
        case BaseType::uint64_t:
            value = arg.value.val_uint64_t;
            argument_stream << arg.value.val_uint64_t;
            break;
        case BaseType::int64_t:
            value = arg.value.val_int64_t;
            argument_stream << arg.value.val_int64_t;
            break;
        case BaseType::float_t:
            if (num_decimal_digits >= 0) {
                argument_stream << std::fixed << std::setprecision(num_decimal_digits) << arg.value.val_float;
            } else {
                argument_stream << arg.value.val_float;
            }
            break;
        case BaseType::invalid: argument_stream << "(unknown)"; break;
    }

    // add the unit if any
    if (!unit.empty()) {
        string normal_unit = unit;
        if (unit == "m_v")
            normal_unit = "m";

        switch (_event_definition.arguments[argument_idx].type) {
            case BaseType::float_t:
                if (_config.formatters.float_value_with_unit) {
                    string value_with_unit =
                        _config.formatters.float_value_with_unit(arg.value.val_float, num_decimal_digits, unit);
                    argument_stream.str("");
                    argument_stream << value_with_unit;
                } else {
                    argument_stream << " " << normal_unit;
                }
                break;
            case BaseType::invalid: break;
            default:
                if (_config.formatters.int_value_with_unit) {
                    string value_with_unit = _config.formatters.int_value_with_unit((int64_t)value, unit);
                    argument_stream.str("");
                    argument_stream << value_with_unit;
                } else {
                    argument_stream << " " << normal_unit;
                }
                break;
        }
    }

    if (_event_definition.arguments[argument_idx].isEnum()) {
        argument_stream.str("");
        EnumDefinition* enum_def = _event_definition.arguments[argument_idx].enum_def;
        if (enum_def->is_bitfield) {
            int bits = baseTypeSize(enum_def->type) * 8;
            bool had_bit = false;
            for (int i = 0; i < bits; ++i) {
                uint64_t bit = ((uint64_t)1 << i);
                if ((value & bit)) {
                    if (had_bit) {
                        argument_stream << "|";  // delimiter
                    }
                    auto entry_iter = enum_def->entries.find(bit);
                    if (entry_iter == enum_def->entries.end()) {
                        argument_stream << "(unknown: " << value << ")";
                    } else {
                        argument_stream << entry_iter->second.description;
                    }
                    had_bit = true;
                }
            }
        } else {
            auto entry_iter = enum_def->entries.find(value);
            if (entry_iter == enum_def->entries.end()) {
                argument_stream << "(unknown: " << value << ")";
            } else {
                argument_stream << entry_iter->second.description;
            }
        }
    }

    return argument_stream.str();
}

string ParsedEvent::processMessage(const string& message) const
{
    /*
     * Supported parsing:
     * - characters can be escaped with \, e.g. '\<', '\{'
     * - tags:
     *   - <profile name="[!]NAME">CONTENT</profile>
     *   - <a [href="URL"]>CONTENT</a>
     *     if href is not found, use CONTENT as url
     *   - <param>PARAM_NAME</param>
     *   - unknown tags are ignored (including content)
     *   - no nested tags of the same type
     * - arguments: following python syntax, with 1-based indexing (instead of 0)
     *   and custom types (units)
     *   - general form: {ARG_IDX[:.NUM_DECIMAL_DIGITS][UNIT]}
     *     UNIT:
     *     - m: horizontal distance in meters
     *     - m_v: vertical distance in meters
     *     - m^2: area in m^2
     *     - m/s: speed in m/s
     *     - C: temperature in degrees celcius
     * - returned string will be trimmed (removed whitespace)
     */

    string ret = message;

    for (int i = 0; i < (int)ret.size(); ++i) {
        if (ret[i] == '\\') {
            ret.erase(i, 1);

        } else if (ret[i] == '<') {  // parse tags
            size_t tag_end_pos = find(ret, "> ", i);
            size_t tag_content_start = find(ret, ">", i);

            if (tag_end_pos == string::npos || tag_content_start == string::npos) {
                continue;
            }

            string tag = ret.substr(i + 1, tag_end_pos - i - 1);
            size_t closing_tag_pos = findClosingTag(ret, tag_end_pos, tag);
            if (closing_tag_pos == string::npos) {
                continue;
            }

            string tag_content = ret.substr(tag_content_start + 1, closing_tag_pos - tag_content_start - 1);
            LIBEVENTS_PARSER_DEBUG_PRINTF("found tag=%s, content=%s\n", tag.c_str(), tag_content.c_str());

            // arguments
            string argument_name;
            string argument;
            if (ret[tag_end_pos] == ' ') {
                string arguments = ret.substr(tag_end_pos + 1, tag_content_start - tag_end_pos - 1);
                // just extract the first argument
                size_t equal_char = arguments.find("=\"");
                if (equal_char != string::npos) {
                    size_t endof_arg = find(arguments, "\"", equal_char + 2);

                    if (endof_arg != string::npos) {
                        argument_name = arguments.substr(0, equal_char);
                        argument = arguments.substr(equal_char + 2, endof_arg - equal_char - 2);
                        LIBEVENTS_PARSER_DEBUG_PRINTF("argument: name=%s, value=%s\n", argument_name.c_str(),
                                                      argument.c_str());
                    }
                }
            }

            size_t num_skip = 0;
            if (tag == "param") {
                tag_content = _config.formatters.param(tag_content);
                num_skip = tag_content.size();  // skip whatever we get back, don't try to parse
            } else if (tag == "a") {
                if (argument.empty() || argument_name != "href") {
                    argument = tag_content;
                }
                tag_content = _config.formatters.url(tag_content, argument);
                num_skip = tag_content.size();  // skip whatever we get back, don't try to parse
            } else if (tag == "profile") {
                if (argument_name == "name" && argument.size() > 0) {
                    if (argument[0] == '!') {
                        if (_config.profile == argument.substr(1)) {
                            tag_content = "";
                        }

                    } else {
                        if (_config.profile != argument) {
                            tag_content = "";
                        }
                    }
                }
            } else {
                // unknown tag: remove, including content
                tag_content = "";
            }

            ret = ret.substr(0, i) + tag_content + ret.substr(closing_tag_pos + tag.size() + 3);
            i = i - 1 + num_skip;

        } else if (ret[i] == '{') {  // parse arguments printing
            size_t format_end_pos = ret.find('}', i);
            if (format_end_pos == string::npos) {
                continue;
            }

            const string format = ret.substr(i + 1, format_end_pos - i - 1);
            char* argument_end = nullptr;
            int argument_idx = strtol(format.c_str(), &argument_end, 0) - 1;
            if (argument_end == nullptr || argument_idx < 0 ||
                argument_idx >= (int)_event_definition.arguments.size()) {
                continue;
            }

            size_t index = argument_end - format.c_str();

            // check for decimal digits [:.NUM_DECIMAL_DIGITS]
            int num_decimal_digits = -1;
            if (index < format.length() && format[index] == ':') {
                ++index;
                if (index < format.length() && format[index] == '.') {
                    num_decimal_digits = strtol(format.c_str() + index + 1, &argument_end, 0);
                    if (argument_end) {
                        index = argument_end - format.c_str();
                    }
                }
            }

            // check for unit
            string unit_str;
            if (index < format.length()) {
                unit_str = format.substr(index);
            }

            LIBEVENTS_PARSER_DEBUG_PRINTF("printf format: %s, arg idx=%i, digits=%i, unit=%s\n", format.c_str(),
                                          argument_idx, num_decimal_digits, unit_str.c_str());
            string argument = getFormattedArgument(argument_idx, num_decimal_digits, unit_str);
            ret = ret.substr(0, i) + argument + ret.substr(format_end_pos + 1);
            i += argument.size() - 1;
        }
    }

    trim(ret);
    return ret;
}

ParsedEvent::Argument ParsedEvent::argumentValue(int index) const
{
    int offset = 0;
    for (int i = 0; i < index; ++i) {
        offset += baseTypeSize(_event_definition.arguments[i].type);
    }
    int type_size = baseTypeSize(_event_definition.arguments[index].type);
    if (offset + type_size > (int)sizeof(_event.arguments)) {
        return {};
    }
    Argument value;
    memcpy(&value.value, _event.arguments + offset, type_size);
    return value;
}

bool Parser::loadDefinitionsFile(const string& definitions_file, translate_func translate)
{
    try {
        ifstream stream(definitions_file);
        json j;
        stream >> j;
        return loadDefinitions(j, translate);

    } catch (const json::exception& e) {
        LIBEVENTS_PARSER_DEBUG_PRINTF("json exception: %s\n", e.what());
    }
    return false;
}

bool Parser::loadDefinitions(const string& definitions, translate_func translate)
{
    try {
        return loadDefinitions(json::parse(definitions), translate);

    } catch (const json::exception& e) {
        LIBEVENTS_PARSER_DEBUG_PRINTF("json exception: %s\n", e.what());
    }
    return false;
}

bool Parser::loadDefinitions(const json& j, translate_func translate)
{
    // version check
    if (!j.contains("version") || j["version"].get<int>() < 1) {
        return false;
    }

    LIBEVENTS_PARSER_DEBUG_PRINTF("File version: %i\n", j["version"].get<int>());

    if (!j.contains("components")) {
        return true;
    }
    try {
        // load enums first
        for (const auto& comp_iter : j["components"].items()) {
            const auto& component = comp_iter.value();

            if (!component.contains("component_id") || !component.contains("namespace")) {
                // invalid definition
                continue;
            }

            string event_namespace = component["namespace"].get<string>();
            LIBEVENTS_PARSER_DEBUG_PRINTF("Component: ns=%s\n", event_namespace.c_str());

            if (component.contains("enums")) {
                for (const auto& enum_iter : component["enums"].items()) {
                    const auto& event_enum = enum_iter.value();

                    std::unique_ptr<EnumDefinition> enum_def{new EnumDefinition{}};
                    enum_def->event_namespace = event_namespace;
                    enum_def->name = event_enum.at("name").get<string>();
                    string enum_type = event_enum.at("type").get<string>();

                    if (event_enum.contains("description")) {
                        enum_def->description = translate(event_enum.at("description").get<string>());
                    }
                    if (event_enum.contains("is_bitfield")) {
                        enum_def->is_bitfield = event_enum.at("is_bitfield").get<bool>();
                    }

                    LIBEVENTS_PARSER_DEBUG_PRINTF("Enum: %s, type=%s\n", enum_def->name.c_str(), enum_type.c_str());
                    enum_def->type = fromString(enum_type);

                    if (enum_def->type == BaseType::invalid) {
                        LIBEVENTS_PARSER_DEBUG_PRINTF("Ignoring enum with invalid type\n");
                        continue;
                    }

                    // entries
                    if (event_enum.contains("entries")) {
                        for (const auto& entry_iter : event_enum["entries"].items()) {
                            const auto& entry = entry_iter.value();
                            uint64_t value = entry.at("value").get<uint64_t>();
                            EnumEntryDefinition entry_def;
                            entry_def.name = entry.at("name").get<string>();
                            entry_def.description = translate(entry.at("description").get<string>());
                            enum_def->entries.emplace(std::make_pair(value, entry_def));
                            LIBEVENTS_PARSER_DEBUG_PRINTF("  value: %" PRIu64 ", name=%s\n", value,
                                                          entry_def.name.c_str());
                        }
                    }

                    _enums.emplace(std::make_pair(event_namespace + "::" + enum_def->name, std::move(enum_def)));
                }
            }
        }

        // load events
        for (const auto& comp_iter : j["components"].items()) {
            const auto& component = comp_iter.value();

            if (!component.contains("component_id") || !component.contains("namespace")) {
                // invalid definition
                continue;
            }

            string event_namespace = component["namespace"].get<string>();
            uint32_t component_id = component["component_id"].get<uint32_t>() & 0xff;
            LIBEVENTS_PARSER_DEBUG_PRINTF("Component: id=%i, ns=%s\n", component_id, event_namespace.c_str());

            if (component.contains("event_groups")) {
                for (const auto& event_group_iter : component["event_groups"].items()) {
                    const auto& event_group = event_group_iter.value();
                    string event_group_name = event_group.at("name").get<string>();
                    LIBEVENTS_PARSER_DEBUG_PRINTF("Event group: %s\n", event_group_name.c_str());

                    for (const auto& event_iter : event_group["events"].items()) {
                        const auto& event = event_iter.value();

                        std::unique_ptr<EventDefinition> event_def{new EventDefinition{}};
                        event_def->event_namespace = event_namespace;
                        event_def->group_name = event_group_name;

                        event_def->name = event.at("name").get<string>();
                        event_def->message = translate(event.at("message").get<string>());

                        if (event.contains("description")) {
                            event_def->description = translate(event.at("description").get<string>());
                        }

                        event_def->id = event.at("sub_id").get<uint32_t>() | (component_id << 24);
                        LIBEVENTS_PARSER_DEBUG_PRINTF("  Event: %s, ID=0x%08x, msg: %s\n", event_def->name.c_str(),
                                                      event_def->id, event_def->message.c_str());

                        // arguments
                        bool invalid_type = false;
                        if (event.contains("arguments")) {
                            for (const auto& arg_iter : event["arguments"].items()) {
                                const auto& arg = arg_iter.value();
                                EventArgumentDefinition arg_def{};
                                arg_def.name = arg.at("name").get<string>();

                                if (arg.contains("description")) {
                                    arg_def.description = translate(arg.at("description").get<string>());
                                }

                                string type = arg.at("type").get<string>();
                                LIBEVENTS_PARSER_DEBUG_PRINTF("    Arg: %s, type=%s\n", arg_def.name.c_str(),
                                                              type.c_str());
                                BaseType base_type = fromString(type);
                                if (base_type == BaseType::invalid) {
                                    // try to find the enum definition for it
                                    arg_def.enum_def = findEnumDefinition(event_namespace, type);

                                    if (!arg_def.enum_def) {
                                        LIBEVENTS_PARSER_DEBUG_PRINTF("Error: invalid type, no enum found\n");
                                        invalid_type = true;
                                    } else {
                                        arg_def.type = arg_def.enum_def->type;
                                    }
                                } else {
                                    arg_def.type = base_type;
                                }
                                event_def->arguments.emplace_back(arg_def);
                            }
                        }

                        if (invalid_type) {
                            continue;
                        }

                        if (_events.find(event_def->id) != _events.end()) {
                            LIBEVENTS_PARSER_DEBUG_PRINTF("Error: event already exists, ignoring\n");
                            continue;
                        }

                        _events.emplace(std::make_pair(event_def->id, std::move(event_def)));
                    }
                }
            }

            if (component.contains("supported_protocols")) {
                if (_supported_protocols.find(component_id) == _supported_protocols.end()) {
                    _supported_protocols[component_id] = {};
                }
                set<string>& supported_components = _supported_protocols[component_id];
                for (const auto& proto_iter : component["supported_protocols"].items()) {
                    const auto& proto = proto_iter.value().get<string>();
                    supported_components.insert(proto);
                }
            }
        }

    } catch (const json::exception& e) {
        LIBEVENTS_PARSER_DEBUG_PRINTF("json exception: %s\n", e.what());
        return false;
    }

    return true;
}

EnumDefinition* Parser::findEnumDefinition(const std::string& event_namespace, const std::string& type)
{
    string type_namespace;
    string type_name;
    size_t found = type.find("::");

    if (found == string::npos) {
        type_namespace = event_namespace;
        type_name = type;
    } else {
        type_namespace = type.substr(0, found);
        type_name = type.substr(found + 2);
    }

    for (const auto& enum_def : _enums) {
        if (enum_def.second->event_namespace == type_namespace && enum_def.second->name == type_name) {
            return enum_def.second.get();
        }
    }
    return nullptr;
}

unique_ptr<ParsedEvent> Parser::parse(const EventType& event)
{
    auto iter = _events.find(event.id);
    if (iter == _events.end()) {
        return nullptr;
    }
    return unique_ptr<ParsedEvent>(new ParsedEvent(event, _config, *iter->second.get(), _enums));
}

set<string> Parser::supportedProtocols(uint8_t component_id)
{
    auto iter = _supported_protocols.find(component_id);
    if (iter == _supported_protocols.end())
        return {};
    return iter->second;
}

}  // namespace parser
}  // namespace events
