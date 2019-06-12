#!/bin/env julia
# Draft of the julia library for CaosDB
# A. Schlemmer, 04/2019

module CaosDB

import HTTP.URIs: escapeuri
import HTTP: request
import EzXML: ElementNode, TextNode, XMLDocument, link!

# Type for managing the connection
# --------------------------------
# baseurl: The base url of your server. Example: "https://localhost:8887/playground/"
# cacert: The path to a certificate pem file. If left empty no custom certificate will be used.
# cookiestring: The cookiestring which will be set by the login function after logging in to caosdb.
# verbose: When set to true the underlying curl library will respond more verbosively. Can be used for debugging.
# usec: When set to true the c++ library will be used instead of the julia module HTTP
mutable struct Connection
    baseurl::Union{Missing,String}
    cacert::Union{Missing,String}
    cookiestring::Union{Missing,String}
    verbose::Bool
    usec::Bool
end

# abstract type Datatype end
# struct Integer <: Datatype end
# struct Double <: Datatype end
# struct Text <: Datatype end
# struct Datetime <: Datatype end
# struct Date <: Datatype end
# struct Boolean <: Datatype end
# struct Reference <: Datatype end

# abstract type Role end
# abstract type RecordTypeOrRecord <: Role end
# struct RecordType <: RecordTypeOrRecord end
# struct Record <: RecordTypeOrRecord end
# struct Property <: Role end
# struct File <: Role end

mutable struct Entity
    role::String
    id::Union{Missing,Int64}
    name::Union{Missing,String}
    value::Union{Missing,String}
    parents::Vector{Entity}
    properties::Vector{Entity}
    datatype::Union{Missing,String,Entity}
    unit::Union{Missing,String}
    description::Union{Missing,String}
end

global ID_COUNTER = 0

function next_id()
    global ID_COUNTER
    ID_COUNTER -= 1
    return ID_COUNTER
end

Entity(role; id=next_id(), name=missing, value=missing,
           parents=Vector{Entity}(), properties=Vector{Entity}(), datatype=missing,
           unit=missing, description=missing) = Entity(role, id, name, value,
                                                           parents, properties, datatype, unit,
                                                           description)

Property(;id=next_id(), name=missing, value=missing, parents=Vector{Entity}(),
                           datatype=missing,
                           unit=missing) = Entity("Property"; id=id, name=name, value=value, parents=parents,
                      properties=Vector{Entity}(), datatype=datatype, unit=unit)

Record(;id=next_id(), name=missing, parents=Vector{Entity}(),
           properties=Vector{Entity}()) = Entity("Record"; id=id, name=name, parents=parents, properties=properties)

RecordType(;id=next_id(), name=missing, parents=Vector{Entity}(), properties=Vector{Entity}()) = Entity("RecordType"; id=id, name=name, parents=parents, properties=properties)

function sub_to_node(subs::Vector{Entity}, name::String, node)
    """
    Helper function
    Checks whether the list subs has length greater 0.
    Afterwards creates a new node with given name,
    creates subnodes for each element of subs,
    finally links the new node to node.
    """
    if (length(subs) > 0)
        parentnode = ElementNode(name)
        for par in subs
            subnode = entity_to_xml(par)
            link!(parentnode, subnode)
        end
        link!(node, parentnode)
    end
end

macro addnonmissingattribute(node, entity, entfield)
    """
    Add an attribute to an xml node if it is not missing
    in the entity.
    """
    t = esc(Symbol(entity))
    s = Symbol(entfield)
    n = esc(Symbol(node))
    quote
        if (!ismissing($(t).$(s)))
            $(n)[$(entfield)] = $(t).$(s)
        end
    end
end

function xml2str(xml)
    """
    Convert an xml node or document to a string.
    """
    return sprint(print, xml)
end

entities_to_xml(entities::Vector{Entity}) = [entity_to_xml(entity) for entity in entities]

function entity_to_xml(entity::Entity)
    """
    Converts an entity representation to XML.
    This is needed for passing the XML in the body of the HTTP request to the server.
    """
    
    node = ElementNode(entity.role)
    sub_to_node(entity.parents, "Parents", node)
    sub_to_node(entity.properties, "Properties", node)
    @addnonmissingattribute("node", "entity", "name")
    @addnonmissingattribute("node", "entity", "id")
    @addnonmissingattribute("node", "entity", "unit")
    @addnonmissingattribute("node", "entity", "description")
    if (!ismissing(entity.value))
        vlnode = TextNode(entity.value)
        link!(node, vlnode)
    end
    if (!ismissing(entity.datatype))
        # TODO: this is not a good solution yet, because of list types
        if (typeof(entity.datatype) == Entity)
            node["datatype"] = entity.datatype.name
        else
            node["datatype"] = entity.datatype
        end
        
    end
    
    
    return(node)
end

function xml_to_entity(xml)
    return xml
    # doc = parse_xml(xml)
    # ... process xml and create a container
end




function passpw(identifier)
    return unsafe_string(ccall((:pass_pw, joinpath(@__DIR__, "libcaoslib")), Cstring, (Cstring,), identifier))
end

caoslib_path = joinpath(@__DIR__, "libcaoslib")

function _base_login(username, password, baseurl, cacert, verbose, usec)
    response = unsafe_string(ccall((:login, "./libcaoslib"), Cstring,
                                   (Cstring, Cstring, Cstring, Cstring, Cuchar),
                                   username, password,
                                   baseurl, cacert, verbose))
    if response[1:6] == "Error:"
        error(response[7:end])
    end    
    return response
end    

# TODO: turn the underscore functions into error checking functions like seen above
#       using a macro.
# _base_login = @errorchecking(_base_login)

# why is this not working: joinpath(@__DIR__, "libcaoslib")

function _base_get(url, cookiestring, baseurl, cacert, verbose)
    return unsafe_string(ccall((:get, "./libcaoslib"), Cstring,
                               (Cstring, Cstring, Cstring, Cstring, Cuchar),
                               url, cookiestring,
                               baseurl, cacert, verbose))
end

function _base_delete(url, cookiestring, baseurl, cacert, verbose)
    return unsafe_string(ccall((:del, "./libcaoslib"), Cstring,
                               (Cstring, Cstring, Cstring, Cstring, Cuchar),
                               url, cookiestring,
                               baseurl, cacert, verbose))
end


function _base_put(url, cookiestring, body, baseurl, cacert, verbose)
    return unsafe_string(ccall((:put, "./libcaoslib"), Cstring,
                               (Cstring, Cstring, Cstring, Cstring, Cstring, Cuchar),
                               url, cookiestring, body,
                               baseurl, cacert, verbose))
end

function _base_post(url, cookiestring, body, baseurl, cacert, verbose)
    return unsafe_string(ccall((:post, "./libcaoslib"), Cstring,
                               (Cstring, Cstring, Cstring, Cstring, Cstring, Cuchar),
                               url, cookiestring, body,
                               baseurl, cacert, verbose))
end

function login(username, password, connection::Connection)
    if connection.usec
        connection.cookiestring = _base_login(username, password,
                                              connection.baseurl,
                                              connection.cacert,
                                              connection.verbose)
    else
        verbose = 0
        if connection.verbose
            verbose = 2
        end
        
        request("POST", connection.baseurl * "login", [],
                "username="*username*"&password="*password;
                verbose=verbose,
                require_ssl_verification=false,
                cookies=Dict{String,String}("type" => "ok"))
    end
end

function get(url, connection::Connection)
    if connection.usec
        return _base_get(url,
                         connection.cookiestring,
                         connection.baseurl,
                         connection.cacert,
                         connection.verbose)
    else
        verbose = 0
        if connection.verbose
            verbose = 2
        end
        
        resp = request("GET", connection.baseurl * url;
                       verbose=verbose,
                       require_ssl_verification=false,
                       cookies=Dict{String,String}("type" => "ok"))
        return resp
    end
end

function _delete(url, connection::Connection)
    if connection.usec
        return _base_delete(url,
                        connection.cookiestring,
                        connection.baseurl,
                        connection.cacert,
                            connection.verbose)
    else
        verbose = 0
        if connection.verbose
            verbose = 2
        end
        
        resp = request("DELETE", connection.baseurl * url;
                       verbose=verbose,
                       require_ssl_verification=false,
                       cookies=Dict{String,String}("type" => "ok"))
        return resp
    end
end

function put(url, body, connection::Connection)
    if connection.usec
        return _base_put(url,
                         connection.cookiestring,
                         body,
                         connection.baseurl,
                         connection.cacert,
                         connection.verbose)
    else
        verbose = 0
        if connection.verbose
            verbose = 2
        end
        
        resp = request("PUT", connection.baseurl * url, [], body;
                       verbose=verbose,
                       require_ssl_verification=false,
                       cookies=Dict{String,String}("type" => "ok"))
        return resp
    end
end

function post(url, body, connection::Connection)
    if connection.usec
        return _base_post(url,
                          connection.cookiestring,
                          body,
                          connection.baseurl,
                          connection.cacert,
                          connection.verbose)
    else
        verbose = 0
        if connection.verbose
            verbose = 2
        end
        
        resp = request("POST", connection.baseurl * url, [], body;
                       verbose=verbose,
                       require_ssl_verification=false,
                       cookies=Dict{String,String}("type" => "ok"))
        return resp
    end
end


function query(querystring, connection::Connection)
    return xml_to_entity(get("Entity/?query=" *
                             escapeuri(querystring), connection))
end

# TODO: <Insert>*</Insert> missing

entity_to_querystring(cont::Vector{Entity}) = join([element.name for element in cont], ',')

insert(cont::Vector{Entity}, connection) = post("Entity/", xml2str(entity_to_xml(cont)), connection)
update(cont::Vector{Entity}, connection) = put("Entity/", xml2str(entity_to_xml(cont)), connection)
retrieve(querystring::String, connection::Connection) = xml_to_entity(get("Entity/" * querystring, connection))
delete(querystring::String, connection::Connection) = _delete("Entity/" * querystring, connection)

retrieve(cont::Vector{Entity}, connection) = retrieve(entity_to_querystring(cont), connection)
delete(cont::Vector{Entity}, connection) = delete(entity_to_querystring(cont), connection)


end
