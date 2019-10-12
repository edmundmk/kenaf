

namespace kf
{

/*
    Runtime has the following tables:

        roots       -> map[ value -> refcount ]                    // strong refs to values
        key_names   -> map[ string ]                             // weak ref to strings
        key_map     -> map[ layout, string -> index ]
        addkey_map  -> map[ layout, string -> layout ]
        proto_map   -> map[ object -> layout ]


    Entries in roots table are alive
        - while the refcount is non-zero.

    Entries in key_names table are alive as long as
        - the string is alive
        - any layout with this key is alive

    Layouts are alive while:
        - the layout is the root layout of a prototype and the object is alive.
        - any object using the layout is alive.
        - any layout that is descended from the layout is alive.

    Keys in the key map are alive while
        - the layout is alive

    Keys in the proto map are alive while
        - the object is alive


    key_names has a weak reference to strings.  so resurrection of keys
    requires careful thought.

    key_map has a weak reference to child layouts.  so resurrection of child
    layouts requires careful thought, too.

    maybe, once sweeping has begun, all unmarked objects are dead so you must
    act as if they are dead.


    We can mark roots like any other reference - if it is updated before the
    GC has got to it, then the mutator must ensure the marker gets the state
    at the start of the mark phase.


    Marking marks through the tables:

        - Objects mark their prototype in proto_map.
        - Layouts mark strings.
        -


    The only thing that can remove entries from the key_names, key_map, and
    proto_map tables is the sweeping process.

        - Once objects are swept, the entry in proto_map is removed.



    Objects are sealed once they are used as a prototype.

    Key lookups are cached in selectors, which look like:

        [ key / layout -> slot pointer or index ]

    Cothread stacks are marked eagerly.  This means that the entire state of
    the stack has to be marked before it can be modified.


    Concurrent GC.  On x86, mov gives us all the ordering guarantees we need.
    On ARM, section 6.3:
        http://infocenter.arm.com/help/topic/com.arm.doc.genc007826/Barrier_Litmus_Tests_and_Cookbook_A08.pdf

        create object and construct it
        dmb st
        store pointer to object (relaxed)

        read pointer to object (relaxed)
        access object through pointer

    Is fine.

*/


struct layout_record
{
    // gcword
    // prototype
    // previous
    // keyindex[]
};

struct object_record
{
    // gcword
    // is_sealed?
    // layout
    // slotsv[]
};

struct cobject_record : public object_record
{
    // gcword
    // layout
    // slotsv[]
    // -native
};

struct array : public object_record
{
    // gcword
    // length
    // values[]
};

struct table : public object_record
{
    // gcword
    // length
    // keyval[]
};

struct cothread : public object_record
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

