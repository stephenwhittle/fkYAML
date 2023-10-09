/**
 *  _______   __ __   __  _____   __  __  __
 * |   __| |_/  |  \_/  |/  _  \ /  \/  \|  |     fkYAML: A C++ header-only YAML library
 * |   __|  _  < \_   _/|  ___  |    _   |  |___  version 0.0.1
 * |__|  |_| \__|  |_|  |_|   |_|___||___|______| https://github.com/fktn-k/fkYAML
 *
 * SPDX-FileCopyrightText: 2023 Kensuke Fukutani <fktn.dev@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * @file
 */

#ifndef FK_YAML_DESERIALIZER_HPP_
#define FK_YAML_DESERIALIZER_HPP_

#include <cstdint>
#include <unordered_map>

#include "fkYAML/detail/version_macros.hpp"
#include "fkYAML/detail/lexical_analyzer.hpp"
#include "fkYAML/detail/type_traits.hpp"
#include "fkYAML/exception.hpp"
#include "fkYAML/node.hpp"

/**
 * @namespace fkyaml
 * @brief namespace for fkYAML library.
 */
FK_YAML_NAMESPACE_BEGIN

/**
 * @class basic_deserializer
 * @brief A class which provides the feature of deserializing YAML documents.
 *
 * @tparam BasicNodeType A type of the container for deserialized YAML values.
 */
template <typename BasicNodeType = node>
class basic_deserializer
{
    static_assert(
        detail::is_basic_node<BasicNodeType>::value, "basic_deserializer only accepts (const) basic_node<...>");

    /** A type for sequence node value containers. */
    using sequence_type = typename BasicNodeType::sequence_type;
    /** A type for mapping node value containers. */
    using mapping_type = typename BasicNodeType::mapping_type;
    /** A type for boolean node values. */
    using boolean_type = typename BasicNodeType::boolean_type;
    /** A type for integer node values. */
    using integer_type = typename BasicNodeType::integer_type;
    /** A type for float number node values. */
    using float_number_type = typename BasicNodeType::float_number_type;
    /** A type for string node values. */
    using string_type = typename BasicNodeType::string_type;
    /** A type for the lexical analyzer object used by this deserializer. */
    using lexer_type = detail::lexical_analyzer<BasicNodeType>;

    using lexical_token_t = detail::lexical_token_t;
    using yaml_version_t = detail::yaml_version_t;

public:
    /**
     * @brief Construct a new basic_deserializer object.
     */
    basic_deserializer() = default;

public:
    /**
     * @brief Deserialize a YAML-formatted source string into a YAML node.
     *
     * @param source A YAML-formatted source string.
     * @return BasicNodeType A root YAML node deserialized from the source string.
     */
    BasicNodeType deserialize(const char* const source)
    {
        if (!source)
        {
            throw fkyaml::exception("The given source for deserialization is nullptr.");
        }

        m_lexer.set_input_buffer(source);
        BasicNodeType root = BasicNodeType::mapping();
        m_current_node = &root;

        lexical_token_t type = m_lexer.get_next_token();
        while (type != lexical_token_t::END_OF_BUFFER)
        {
            switch (type)
            {
            case lexical_token_t::KEY_SEPARATOR:
                if (m_node_stack.empty() || !m_node_stack.back()->is_mapping())
                {
                    throw fkyaml::exception("A key separator found while a value token is expected.");
                }
                if (m_current_node->is_sequence() && m_current_node->size() == 1)
                {
                    // make sequence node to mapping node.
                    string_type tmp_str = m_current_node->operator[](0).to_string();
                    m_current_node->operator[](0) = BasicNodeType::mapping();
                    m_node_stack.emplace_back(m_current_node);
                    m_current_node = &(m_current_node->operator[](0));
                    set_yaml_version(*m_current_node);
                    m_current_node->to_mapping().emplace(tmp_str, BasicNodeType());
                    m_node_stack.emplace_back(m_current_node);
                    m_current_node = &(m_current_node->operator[](tmp_str));
                    set_yaml_version(*m_current_node);
                }
                break;
            case lexical_token_t::VALUE_SEPARATOR:
                break;
            case lexical_token_t::ANCHOR_PREFIX: {
                m_anchor_name = m_lexer.get_string();
                m_needs_anchor_impl = true;
                break;
            }
            case lexical_token_t::ALIAS_PREFIX: {
                m_anchor_name = m_lexer.get_string();
                if (m_anchor_table.find(m_anchor_name) == m_anchor_table.end())
                {
                    throw fkyaml::exception("The given anchor name must appear prior to the alias node.");
                }
                assign_node_value(BasicNodeType::alias_of(m_anchor_table.at(m_anchor_name)));
                break;
            }
            case lexical_token_t::COMMENT_PREFIX:
                break;
            case lexical_token_t::YAML_VER_DIRECTIVE: {
                FK_YAML_ASSERT(m_current_node == &root);
                update_yaml_version_from(m_lexer.get_yaml_version());
                set_yaml_version(*m_current_node);
                break;
            }
            case lexical_token_t::TAG_DIRECTIVE:
                // TODO: implement tag directive deserialization.
            case lexical_token_t::INVALID_DIRECTIVE:
                // TODO: should output a warning log. Currently just ignore this case.
                break;
            case lexical_token_t::SEQUENCE_BLOCK_PREFIX:
                if (m_current_node->is_mapping())
                {
                    if (m_current_node->empty())
                    {
                        *m_current_node = BasicNodeType::sequence();
                        break;
                    }

                    // for the second or later mapping items in a sequence node.
                    m_node_stack.back()->to_sequence().emplace_back(BasicNodeType::mapping());
                    m_current_node = &(m_node_stack.back()->to_sequence().back());
                    set_yaml_version(*m_current_node);
                    break;
                }
                break;
            case lexical_token_t::SEQUENCE_FLOW_BEGIN:
                *m_current_node = BasicNodeType::sequence();
                set_yaml_version(*m_current_node);
                break;
            case lexical_token_t::SEQUENCE_FLOW_END:
                m_current_node = m_node_stack.back();
                m_node_stack.pop_back();
                break;
            case lexical_token_t::MAPPING_BLOCK_PREFIX:
                *m_current_node = BasicNodeType::mapping();
                set_yaml_version(*m_current_node);
                break;
            case lexical_token_t::MAPPING_FLOW_BEGIN:
                if (m_current_node->is_mapping())
                {
                    throw fkyaml::exception("Cannot assign a mapping value as a key.");
                }
                *m_current_node = BasicNodeType::mapping();
                set_yaml_version(*m_current_node);
                break;
            case lexical_token_t::MAPPING_FLOW_END:
                if (!m_current_node->is_mapping())
                {
                    throw fkyaml::exception("Invalid mapping flow ending found.");
                }
                m_current_node = m_node_stack.back();
                m_node_stack.pop_back();
                break;
            case lexical_token_t::NULL_VALUE:
                if (m_current_node->is_mapping())
                {
                    add_new_key(m_lexer.get_string());
                    break;
                }

                // Just make sure that the actual value is really a null value.
                m_lexer.get_null();
                assign_node_value(BasicNodeType());
                break;
            case lexical_token_t::BOOLEAN_VALUE:
                if (m_current_node->is_mapping())
                {
                    add_new_key(m_lexer.get_string());
                    break;
                }
                assign_node_value(BasicNodeType::boolean_scalar(m_lexer.get_boolean()));
                break;
            case lexical_token_t::INTEGER_VALUE:
                if (m_current_node->is_mapping())
                {
                    add_new_key(m_lexer.get_string());
                    break;
                }
                assign_node_value(BasicNodeType::integer_scalar(m_lexer.get_integer()));
                break;
            case lexical_token_t::FLOAT_NUMBER_VALUE:
                if (m_current_node->is_mapping())
                {
                    add_new_key(m_lexer.get_string());
                    break;
                }
                assign_node_value(BasicNodeType::float_number_scalar(m_lexer.get_float_number()));
                break;
            case lexical_token_t::STRING_VALUE:
                if (m_current_node->is_mapping())
                {
                    add_new_key(m_lexer.get_string());
                    break;
                }
                assign_node_value(BasicNodeType::string_scalar(m_lexer.get_string()));
                break;
            default:                                                         // LCOV_EXCL_LINE
                throw fkyaml::exception("Unsupported lexical token found."); // LCOV_EXCL_LINE
            }

            type = m_lexer.get_next_token();
        }

        m_current_node = nullptr;
        m_needs_anchor_impl = false;
        m_anchor_table.clear();
        m_node_stack.clear();

        return root;
    }

private:
    /**
     * @brief Add new key string to the current YAML node.
     *
     * @param key a key string to be added to the current YAML node.
     */
    void add_new_key(const string_type& key) noexcept
    {
        m_current_node->to_mapping().emplace(key, BasicNodeType());
        m_node_stack.push_back(m_current_node);
        m_current_node = &(m_current_node->to_mapping().at(key));
    }

    /**
     * @brief Assign node value to the current node.
     *
     * @param node_value A rvalue BasicNodeType object to be assigned to the current node.
     */
    void assign_node_value(BasicNodeType&& node_value) noexcept
    {
        if (m_current_node->is_sequence())
        {
            m_current_node->to_sequence().emplace_back(std::move(node_value));
            set_yaml_version(m_current_node->to_sequence().back());
            if (m_needs_anchor_impl)
            {
                m_current_node->to_sequence().back().add_anchor_name(m_anchor_name);
                m_anchor_table[m_anchor_name] = m_current_node->to_sequence().back();
                m_needs_anchor_impl = false;
                m_anchor_name.clear();
            }
            return;
        }

        // a scalar node
        *m_current_node = std::move(node_value);
        set_yaml_version(*m_current_node);
        if (m_needs_anchor_impl)
        {
            m_current_node->add_anchor_name(m_anchor_name);
            m_anchor_table[m_anchor_name] = *m_current_node;
            m_needs_anchor_impl = false;
            m_anchor_name.clear();
        }
        m_current_node = m_node_stack.back();
        m_node_stack.pop_back();
    }

    /**
     * @brief Set the yaml_version_t object to the given node.
     *
     * @param node A BasicNodeType object to be set the yaml_version_t object.
     */
    void set_yaml_version(BasicNodeType& node) noexcept
    {
        node.set_yaml_version(m_yaml_version);
    }

    /**
     * @brief Update the target YAML version with an input string.
     *
     * @param version_str A YAML version string.
     */
    void update_yaml_version_from(const string_type& version_str) noexcept
    {
        if (version_str == "1.1")
        {
            m_yaml_version = yaml_version_t::VER_1_1;
            return;
        }
        m_yaml_version = yaml_version_t::VER_1_2;
    }

private:
    lexer_type m_lexer {};                                   /** A lexical analyzer object. */
    BasicNodeType* m_current_node = nullptr;                 /** The currently focused YAML node. */
    std::vector<BasicNodeType*> m_node_stack {};             /** The stack of YAML nodes. */
    yaml_version_t m_yaml_version = yaml_version_t::VER_1_2; /** The YAML version specification type. */
    uint32_t m_current_indent_width = 0;                     /** The current indentation width. */
    bool m_needs_anchor_impl = false; /** A flag to determine the need for YAML anchor node implementation */
    string_type m_anchor_name {};     /** The last YAML anchor name. */
    std::unordered_map<std::string, BasicNodeType> m_anchor_table; /** The table of YAML anchor nodes. */
};

/**
 * @brief default YAML document deserializer.
 */
using deserializer = basic_deserializer<>;

FK_YAML_NAMESPACE_END

#endif /* FK_YAML_DESERIALIZER_HPP_ */
