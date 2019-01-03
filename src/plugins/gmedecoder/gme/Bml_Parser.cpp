#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "Bml_Parser.h"

const char * strchr_limited( const char * in, const char * end, char c )
{
    while ( in < end && *in != c ) ++in;
    if ( in < end ) return in;
    else return 0;
}

Bml_Node Bml_Node::emptyNode;

Bml_Node::Bml_Node()
{
    name = 0;
    value = 0;
}

Bml_Node::Bml_Node(char const* name, size_t max_length)
{
    size_t length = 0;
    char const* ptr = name;
    while (*ptr && length < max_length) ++ptr, ++length;
    this->name = new char[ length + 1 ];
    memcpy( this->name, name, length );
    this->name[ length ] = '\0';
    value = 0;
}

Bml_Node::Bml_Node(const Bml_Node &in)
{
    size_t length;
    name = 0;
    if (in.name)
    {
        length = strlen(in.name);
        name = new char[length + 1];
        memcpy(name, in.name, length + 1);
    }
    value = 0;
    if (in.value)
    {
        length = strlen(in.value);
        value = new char[length + 1];
        memcpy(value, in.value, length + 1);
    }
    children = in.children;
}

Bml_Node::~Bml_Node()
{
    delete [] name;
    delete [] value;
}

void Bml_Node::clear()
{
    delete [] name;
    delete [] value;

    name = 0;
    value = 0;
    children.resize( 0 );
}

void Bml_Node::setLine(const char *line, size_t max_length)
{
    delete [] name;
    delete [] value;

    name = 0;
    value = 0;
    
    size_t length = 0;
    const char * end = line;
    while (*end && length < max_length) ++end;

    const char * line_end = strchr_limited(line, end, '\n');
    if ( !line_end ) line_end = end;

    const char * first_letter = line;
    while ( first_letter < line_end && *first_letter <= 0x20 ) first_letter++;

    const char * colon = strchr_limited(first_letter, line_end, ':');
    const char * last_letter = line_end - 1;

    if (colon)
    {
        const char * first_value_letter = colon + 1;
        while (first_value_letter < line_end && *first_value_letter <= 0x20) first_value_letter++;
        last_letter = line_end - 1;
        while (last_letter > first_value_letter && *last_letter <= 0x20) last_letter--;

        value = new char[last_letter - first_value_letter + 2];
        memcpy(value, first_value_letter, last_letter - first_value_letter + 1);
        value[last_letter - first_value_letter + 1] = '\0';

        last_letter = colon - 1;
    }

    while (last_letter > first_letter && *last_letter <= 0x20) last_letter--;

    name = new char[last_letter - first_letter + 2];
    memcpy(name, first_letter, last_letter - first_letter + 1);
    name[last_letter - first_letter + 1] = '\0';
}

Bml_Node& Bml_Node::addChild(const Bml_Node &child)
{
    children.push_back(child);
    return *(children.end() - 1);
}

const char * Bml_Node::getName() const
{
    return name;
}

const char * Bml_Node::getValue() const
{
    return value;
}

void Bml_Node::setValue(char const* value)
{
    delete [] this->value;
    size_t length = strlen( value ) + 1;
    this->value = new char[ length ];
    memcpy( this->value, value, length );
}

size_t Bml_Node::getChildCount() const
{
    return children.size();
}

Bml_Node const& Bml_Node::getChild(size_t index) const
{
    return children[index];
}

Bml_Node & Bml_Node::walkToNode(const char *path, bool use_indexes)
{
    Bml_Node * next_node;
    Bml_Node * node = this;
    while ( *path )
    {
        bool item_found = false;
        size_t array_index = 0;
        const char * array_index_start = strchr( path, '[' );
        const char * next_separator = strchr( path, ':' );
        if ( !next_separator ) next_separator = path + strlen(path);
        if ( use_indexes && array_index_start && array_index_start < next_separator )
        {
            char * temp;
            array_index = strtoul( array_index_start + 1, &temp, 10 );
        }
        else
        {
            array_index_start = next_separator;
        }
        if ( use_indexes )
        {
            for ( std::vector<Bml_Node>::iterator it = node->children.begin(); it != node->children.end(); ++it )
            {
                if ( array_index_start - path == strlen(it->name) &&
                    strncmp( it->name, path, array_index_start - path ) == 0 )
                {
                    next_node = &(*it);
                    item_found = true;
                    if ( array_index == 0 ) break;
                    --array_index;
                }
                if (array_index)
                    item_found = false;
            }
        }
        else
        {
            for ( std::vector<Bml_Node>::iterator it = node->children.end(); it != node->children.begin(); )
            {
                --it;
                if ( next_separator - path == strlen(it->name) &&
                    strncmp( it->name, path, next_separator - path ) == 0 )
                {
                    next_node = &(*it);
                    item_found = true;
                    break;
                }
            }
        }
        if ( !item_found )
        {
            Bml_Node child( path, next_separator - path );
            node = &(node->addChild( child ));
        }
        else
            node = next_node;
        if ( *next_separator )
        {
            path = next_separator + 1;
        }
        else break;
    }
    return *node;
}

Bml_Node const& Bml_Node::walkToNode(const char *path) const
{
    Bml_Node const* next_node;
    Bml_Node const* node = this;
    while ( *path )
    {
        bool item_found = false;
        size_t array_index = ~0;
        const char * array_index_start = strchr( path, '[' );
        const char * next_separator = strchr( path, ':' );
        if ( !next_separator ) next_separator = path + strlen(path);
        if ( array_index_start && array_index_start < next_separator )
        {
            char * temp;
            array_index = strtoul( array_index_start + 1, &temp, 10 );
        }
        else
        {
            array_index_start = next_separator;
        }
        for ( std::vector<Bml_Node>::const_iterator it = node->children.begin(), ite = node->children.end(); it != ite; ++it )
        {
            if ( array_index_start - path == strlen(it->name) &&
                 strncmp( it->name, path, array_index_start - path ) == 0 )
            {
                next_node = &(*it);
                item_found = true;
                if ( array_index == 0 ) break;
                --array_index;
            }
        }
        if ( !item_found ) return emptyNode;
        node = next_node;
        if ( *next_separator )
        {
            path = next_separator + 1;
        }
        else break;
    }
    return *node;
}

void Bml_Parser::parseDocument( const char * source, size_t max_length )
{
    std::vector<size_t> indents;
    std::string last_name;
    std::string current_path;

    document.clear();

    size_t last_indent = ~0;

    Bml_Node node;
    
    size_t length = 0;
    const char * end = source;
    while ( *end && length < max_length ) ++end, ++length;

    while ( source < end )
    {
        const char * line_end = strchr_limited( source, end, '\n' );
        if ( !line_end ) line_end = end;

        if ( node.getName() ) last_name = node.getName();

        node.setLine( source, line_end - source );

        size_t indent = 0;
        while ( source < line_end && *source <= 0x20 )
        {
            source++;
            indent++;
        }

        if ( last_indent == ~0 ) last_indent = indent;

        if ( indent > last_indent )
        {
            indents.push_back( last_indent );
            last_indent = indent;
            if ( current_path.length() ) current_path += ":";
            current_path += last_name;
        }
        else if ( indent < last_indent )
        {
            while ( last_indent > indent && indents.size() )
            {
                last_indent = *(indents.end() - 1);
                indents.pop_back();
                size_t colon = current_path.find_last_of( ':' );
                if ( colon != std::string::npos ) current_path.resize( colon );
                else current_path.resize( 0 );
            }
            last_indent = indent;
        }

        document.walkToNode( current_path.c_str() ).addChild( node );

        source = line_end;
        while ( *source && *source == '\n' ) source++;
    }
}

const char * Bml_Parser::enumValue(std::string const& path) const
{
    return document.walkToNode(path.c_str()).getValue();
}

void Bml_Parser::setValue(std::string const& path, const char *value)
{
    document.walkToNode(path.c_str(), true).setValue(value);
}

void Bml_Parser::setValue(std::string const& path, long value)
{
    std::ostringstream str;
    str << value;
    setValue( path, str.str().c_str() );
}

void Bml_Parser::serialize(std::string & out) const
{
    std::ostringstream strOut;
    serialize(strOut, &document, 0);
    out = strOut.str();
}

void Bml_Parser::serialize(std::ostringstream & out, Bml_Node const* node, unsigned int indent) const
{
    for (unsigned i = 1; i < indent; ++i) out << "  ";

    if ( indent )
    {
        out << node->getName();
        if (node->getValue() && strlen(node->getValue())) out << ":" << node->getValue();
        out << std::endl;
    }

    for (unsigned i = 0, j = node->getChildCount(); i < j; ++i)
    {
        Bml_Node const& child = node->getChild(i);
        if ( (!child.getValue() || !strlen(child.getValue())) && !child.getChildCount() )
            continue;
        serialize( out, &child, indent + 1 );
        if ( indent == 0 ) out << std::endl;
    }
}
