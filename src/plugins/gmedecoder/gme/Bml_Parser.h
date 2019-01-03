#ifndef BML_PARSER_H
#define BML_PARSER_H

#include <vector>
#include <string>
#include <sstream>

class Bml_Node
{
    char * name;
    char * value;

    std::vector<Bml_Node> children;

    static Bml_Node emptyNode;

public:
    Bml_Node();
    Bml_Node(char const* name, size_t max_length = ~0UL);
    Bml_Node(Bml_Node const& in);

    ~Bml_Node();

    void clear();

    void setLine(const char * line, size_t max_length = ~0UL);
    Bml_Node& addChild(Bml_Node const& child);

    const char * getName() const;
    const char * getValue() const;
    
    void setValue(char const* value);

    size_t getChildCount() const;
    Bml_Node const& getChild(size_t index) const;

    Bml_Node & walkToNode( const char * path, bool use_indexes = false );
    Bml_Node const& walkToNode( const char * path ) const;
};

class Bml_Parser
{
    Bml_Node document;

public:
    Bml_Parser() { }

    void parseDocument(const char * document, size_t max_length = ~0UL);

    const char * enumValue(std::string const& path) const;
    
    void setValue(std::string const& path, long value);
    void setValue(std::string const& path, const char * value);

    void serialize(std::string & out) const;

private:
    void serialize(std::ostringstream & out, Bml_Node const* node, unsigned int indent) const;
};

#endif // BML_PARSER_H
