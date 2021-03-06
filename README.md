# CaosDB.jl
CaosDB interface for Julia

Currently WIP.

*Note (2019-10-15):* As XML support in Julia is not working smoothly we are working on providing a json interface. This is currently work in progress.

*Update (2020-05-14):* Problems with EzXML seem to have been solved. This library is working.

# Howto

Import the package:
```julia
using CaosDB
```

Create a connection:
```julia
c = CaosDB.Connection("https://demo.indiscale.com/")
```
The connection stores the address to the CaosDB instance (here: demo.indiscale.com). It is passed on to other functions within the CaosDB package. Note that the trailing slash is important, so "https://demo.indiscale.com" would not work. demo.indiscale.com is the demo instance hosted by IndiScale GmbH. This instance should not be used for writing (e.g. inserting) of data. Please use playground.indiscale.com instead.

## Retrieve and Query

Retrieve an Entity by id:
```julia
CaosDB.retrieve("281", c)
```


Do a simple query:
```julia
doc = CaosDB.query("Find Analysis", c)
```

You can access the individual entries using the `properties` attribute of the entity structs.
`doc[1]` is a *RecordType* in this case and contains only unset properties:
```julia
doc[1].properties[1].value
```

`doc[2]` is a *Record*, so values are set:

```julia
doc[2].properties[3].value
```

## Inserting of RecordTypes and Records

Create a simple property:
```julia
p = CaosDB.Property(name="a", datatype="INTEGER",
                   description="just a property which we can use for a quick test")
```

Create a simple record type:
```julia
rt = CaosDB.RecordType(name="CoolType", properties=[p])
CaosDB.insert([p, rt], c)
```

... of course that does not work. Insufficient permissions.

So let's try as admin:
```julia
c2 = CaosDB.Connection("https://playground.indiscale.com/")
CaosDB.login("admin", "caosdb", c2)
CaosDB.insert([p, rt], c2)
```

Ok, that should have worked! If you run this code twice, you end up having multiple of these types
(this can be configured on the server).


So, what about records?
```julia
r1 = CaosDB.Record(parents=CaosDB.query("Find CoolType", c2),
                   properties=[CaosDB.Property(name="a", value="283")])
res = CaosDB.insert([r1], c2)
```

Now, that a *RecordType* and a *Record* have been inserted, we can easily search for them using CaosDB CQL (See https://gitlab.com/caosdb/caosdb/-/wikis/manuals/CQL/CaosDB%20Query%20Language for details):

```julia
search_results = CaosDB.query("Find CoolType", c2)

# Or, if you are only interested in the record:
search_results = CaosDB.query("Find Record CoolType", c2)

# Access your results:
search_results[1].properties[1].value
```


# About CaosDB

CaosDB is an open source research data managament system (RDMS). It consists of a server application (written in Java) and clients that communicate with the server to insert, retrieve, update and query data.

The server can be accessed using either the WebUI (have a look at a demo instance here: https://demo.indiscale.com), or one of the clients.

## Key Features
CaosDB has some key features that make it especially useful for scientific data management:
- Data insertion is mostly automatic using the data crawler
- A semantic data model allows for continuous adaption to changes in the scientific workflow (e.g. new devices, new types of computer simulation, new analysis software)
- A special query language (CQL), that is intuitive also for non-programmers, which can be accessed from the WebUI or from all client interfaces.

## More Information

Official webpage: https://caosdb.org/

CaosDB article: https://www.mdpi.com/2306-5729/4/2/83/htm

Sourcecode: https://gitlab.com/caosdb

CaosDB-Documentation: https://gitlab.com/caosdb/caosdb/-/wikis/home

Research Group Biomedical Physics Webpage: http://bmp.ds.mpg.de/software/caosdb/

Demo instance hosted by IndiScale: https://demo.indiscale.com
