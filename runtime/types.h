

namespace kf
{

/*
    Runtime has the following tables:

        roots        -> map[ o_value, refcount ]                    // strong refs to values
        keys         -> map[ o_string ]                             // weak ref to strings
        layout_proto -> map[ o_object, o_layout ]                   // weak ref to layout
        layout_links -> map[ [ o_layout, o_string ], o_layout ]     // weak refs to all three?

    roots keep values alive
    strings keep their entry in keys alive
    layout_proto is kept alive as long as the _value_ is alive.  The value also
        has a strong reference to the key.
    layout_proto is kept alive as long as the

*/


struct layout
{
    // gcword
    // prototype
    // previous
    // keyindex[]
};

struct object
{
    // gcword
    // is_sealed?
    // layout
    // slotsv[]
};

struct cobject
{
    // gcword
    // layout
    // slotsv[]
    // -native
};

struct array
{
    // gcword
    // length
    // values[]
};

struct table
{
    // gcword
    // length
    // keyval[]
};

struct cothread
{
    // gcword
    // mark
    // framev[]
    // stackv[]
    // upstkv[]
};

struct function
{
    // gcword
    // program
    // upvals[ n ]
};

struct program
{
    // gcword
    // constants[]
    // programs[]
    // -code
};

struct code /* leaf */
{

};

struct cfunction /* leaf */
{
    // gcword
    // -native
    // -native
};

struct string /* leaf */
{
    // gcword
    // is_key
    // length
    // hash
    // chars[]
};



}

