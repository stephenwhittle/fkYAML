//  _______   __ __   __  _____   __  __  __
// |   __| |_/  |  \_/  |/  _  \ /  \/  \|  |     fkYAML: A C++ header-only YAML library (supporting code)
// |   __|  _  < \_   _/|  ___  |    _   |  |___  version 0.3.1
// |__|  |_| \__|  |_|  |_|   |_|___||___|______| https://github.com/fktn-k/fkYAML
//
// SPDX-FileCopyrightText: 2023 Kensuke Fukutani <fktn.dev@gmail.com>
// SPDX-License-Identifier: MIT

#include <fstream>
#include <iostream>
#include <string>
#include <fkYAML/node.hpp>

// Test result codes.
static const int TEST_RESULT_OK = 0;
static const int TEST_RESULT_NG = 1;

// Definitions of indexes of command line arguments.
static const int TEST_DIR_PREFIX_INDEX = 1;


fkyaml::node parse_events(std::istream& is)
{
    constexpr int _eof = std::istream::traits_type::eof();
    std::string input = std::string(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>());
    std::size_t counts = 0;

    fkyaml::node root = fkyaml::node::mapping();
    fkyaml::node* p_current = &root;

    while (true)
    {
        if (std::strncmp(&input[counts], "+STR", 4) == 0)
        {
            counts += 4;
            continue;
        }
        if (std::strncmp(&input[counts], "-STR", 4) == 0)
        {
            counts += 4;
            continue;
        }
        if (std::strncmp(&input[counts], "+DOC", 4) == 0)
        {
            counts += 4;
            continue;
        }
        if (std::strncmp(&input[counts], "-DOC", 4) == 0)
        {
            counts += 4;
            continue;
        }
        if (std::strncmp(&input[counts], "+MAP", 4) == 0)
        {
            counts += 4;
            continue;
        }
        if (std::strncmp(&input[counts], "-MAP", 4) == 0)
        {
            counts += 4;
            continue;
        }
        if (std::strncmp(&input[counts], "+SEQ", 4) == 0)
        {
            counts += 4;
            continue;
        }
        if (std::strncmp(&input[counts], "-SEQ", 4) == 0)
        {
            counts += 4;
            continue;
        }
        if (std::strncmp(&input[counts], "=VAL", 4) == 0)
        {
            counts += 5; // "=VAL"(4) + space(1)

            fkyaml::node::node_t type {fkyaml::node::node_t::NULL_OBJECT};

            if (std::strncmp(&input[counts], "<", 1) == 0)
            {
                if (std::strncmp(&input[counts], "<tag:yaml.org,2002:seq>", 23) == 0)
                {
                    counts += 24; // tag(23) + space(1)
                    type = fkyaml::node::node_t::SEQUENCE;
                }
                else if (std::strncmp(&input[counts], "<tag:yaml.org,2002:map>", 23) == 0)
                {
                    counts += 24; // tag(23) + space(1)
                    type = fkyaml::node::node_t::MAPPING;
                }
                else if (std::strncmp(&input[counts], "<tag:yaml.org,2002:null>", 24) == 0)
                {
                    counts += 25; // tag(24) + space(1)
                    type = fkyaml::node::node_t::NULL_OBJECT;
                }
                else if (std::strncmp(&input[counts], "<tag:yaml.org,2002:bool>", 24) == 0)
                {
                    counts += 25; // tag(24) + space(1)
                    type = fkyaml::node::node_t::BOOLEAN;
                }
                else if (std::strncmp(&input[counts], "<tag:yaml.org,2002:int>", 23) == 0)
                {
                    counts += 24; // tag(23) + space(1)
                    type = fkyaml::node::node_t::INTEGER;
                }
                else if (std::strncmp(&input[counts], "<tag:yaml.org,2002:float>", 25) == 0)
                {
                    counts += 26; // tag(25) + space(1)
                    type = fkyaml::node::node_t::FLOAT_NUMBER;
                }
                else if (std::strncmp(&input[counts], "<tag:yaml.org,2002:str>", 23) == 0)
                {
                    counts += 24; // tag(23) + space(1)
                    type = fkyaml::node::node_t::STRING;
                }
            }

            // TODO: implement this.

            continue;
        }
    }
}


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cout << "Invalid command line arguments.\n";
        return TEST_RESULT_NG;
    }

    std::string test_dir(argv[TEST_DIR_PREFIX_INDEX]);
    std::cout << "test dir: " << test_dir << "\n";

    std::ifstream label_file(test_dir + "/===");
    if (label_file)
    {
        std::cout << "label: ";
        std::string label;
        std::getline(label_file, label);
        std::cout << label << "\n";
    }

    std::ifstream yaml_data_file(test_dir + "/in.yaml");
    if (!yaml_data_file)
    {
        std::cout << "Failed to open yaml data file. path: " << test_dir << "/in.yaml\n";
        return TEST_RESULT_NG;
    }

    std::ifstream error_file(test_dir + "/error");
    bool is_valid = !error_file.is_open();

    std::ifstream json_data_file(test_dir + "/in.json");

    int result = TEST_RESULT_OK;
    // fkyaml::node expected;

    try
    {
        fkyaml::node actual_from_yaml;
        yaml_data_file >> actual_from_yaml;
        // if (actual_from_yaml != expected)
        // {
        //     result = TEST_RESULT_NG;
        // }

        if (json_data_file)
        {
            // fkyaml::node actual_from_json;
            // json_data_file >>actual_from_json;

            // if (actual_from_json != expected)
            // {
            //     result = TEST_RESULT_NG;
            // }
        }
    }
    catch (const fkyaml::exception& e)
    {
        if (is_valid)
        {
            std::cout << "fkYAML error: " << e.what() << "\n";
            result = TEST_RESULT_NG;
        }
    }
    catch (const std::exception& e)
    {
        if (is_valid)
        {
            std::cout << "Unexpected error: " << e.what() << "\n";
            result = TEST_RESULT_NG;
        }
    }

    return result;
}