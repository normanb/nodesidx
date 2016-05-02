NodeSidx
=========

** Node.js [libspatialindex](https://libspatialindex.github.io/) binding**

<a name="platforms"></a>
Tested & supported platforms
----------------------------

  * **Mac OS**
    Tested
  * **Linux**
    Should work, not tested
  * **Windows**
    libspatialindex/libspatialindex.gyp will require a Windows
    version of the `find` command line utility

`npm install`

`npm test`

<a name="api"></a>
## API

  * <a href="#ctor"><code><b>SpatialIndex()</b></code></a>
  * <a href="#spatialindex_open"><code><b>SpatialIndex#open()</b></code></a>
  * <a href="#spatialindex_insert"><code><b>SpatialIndex#insert()</b></code></a>
  * <a href="#spatialindex_intersects"><code><b>SpatialIndex#intersects()</b></code></a>
  * <a href="#spatialindex_bounds"><code><b>SpatialIndex#bounds()</b></code></a>
  * <a href="#spatialindex_delete"><code><b>SpatialIndex#delete()</b></code></a>


--------------------------------------------------------
<a name="ctor"></a>
### SpatialIndex(options)

<code>SpatialIndex()</code> returns a new **SpatialIndex** instance. `options` is a JSON object containing the following configuration parameters

* `'type'` : (string, default: "rtree")
* `'storage'`: (string, default: "memory"): If `'storage'` is "file" then the `'filename'` parameter is required
* `'filename'`: (string): Path to index file if storage is "file"
* `'dimension'`: (integer, default: 2): either 2 (xy) or 3 (xyz)

--------------------------------------------------------
<a name="spatialindex_open"></a>
### SpatialIndex#open(callback)
<code>open()</code> is an instance method on an existing SpatialIndex object.

The `callback` function will be called with no arguments when the database has been successfully opened, or with a single `error` argument if the open operation failed for any reason.

* `'type'` : (string, default: "rtree")
* `'storage'`: (string, default: "memory"): If `'storage'` is "file" then the `'filename'` parameter is required
* `'filename'`: (string): Path to index file if storage is "file"
* `'dimension'`: (integer, default: 2): either 2 (xy) or 3 (xyz)

--------------------------------------------------------
<a name="spatialindex_insert"></a>
### SpatialIndex#insert(id, mins, maxs, data, callback)
<code>insert()</code> is an instance method on an existing SpatialIndex object. The `callback` function will be called with no arguments if the operation is successful or with a single `error` argument if the operation failed for any reason.

* `'id'` : (integer): Identifier for this item
* `'mins'`: (Array): [minx, miny, (minz)]
* `'maxs'`: (Array): [maxx, maxy, (maxz)]
* `'data'`: (Buffer, default: null): Buffer of data to associated with this item

--------------------------------------------------------
<a name="spatialindex_intersects"></a>
### SpatialIndex#intersects(mins, maxs, resultOffset, resultLength, callback)
<code>intersects()</code> is an instance method on an existing SpatialIndex object, used to query the index for items within
a bounding box.

The `callback` function will be called with a single `error` if the operation failed for any reason.

If successful the first argument will be `null` and the second argument will be an array of JSON objects of the form;

```
{
  "id" : someIdInteger,
  "data": "someDataBuffer"  
}
```
* `'mins'`: (Array): [minx, miny, (minz)]
* `'maxs'`: (Array): [maxx, maxy, (maxz)]
* `'resultOffset'`: (Number, default: 0)
* `'resultLimit'`: (Number, default: null)

--------------------------------------------------------
<a name="spatialindex_bounds"></a>
### SpatialIndex#bounds(callback)
<code>bounds()</code> is an instance method on an existing SpatialIndex object, used to fetch the bounding box for the index.

The result will be an Array [minx, miny, (minz), maxx, maxy, (maxz)]

The `callback` function will be called with a single `error` if the operation failed for any reason.

If successful the first argument will be `null` and the second argument will be an Array([minx, miny, (minz), maxx, maxy, (maxz)])


--------------------------------------------------------
<a name="spatialindex_delete"></a>
### SpatialIndex#delete(id, mins, maxs, callback)
<code>delete()</code> is an instance method on an existing SpatialIndex object, used to delete items from the index

* `'id'` : (integer): Identifier for this item
* `'mins'`: (Array): [minx, miny, (minz)]
* `'maxs'`: (Array): [maxx, maxy, (maxz)]

The `callback` function will be called with no arguments if the operation is successful or with a single `error` argument if the operation failed for any reason.

--------------------------------------------------------

<a name="support"></a>
Getting support
---------------

You can find help in using [libspatialindex](https://libspatialindex.github.io/) in Node.js:

 * **GitHub:** you're welcome to open an issue here on this GitHub repository if you have a question.

<a name="contributing"></a>
Contributing
------------

NodeSidx is an **OPEN Open Source Project**. This means that:

> Individuals making significant and valuable contributions are given commit-access to the project to contribute as they see fit. This project is more like an open wiki than a standard guarded open source project.

<a name="license"></a>
License &amp; copyright
-------------------

**NodeSidx** is licensed under the Apache Software Version 2.0 license.

*NodeSidx builds on the excellent work of [libspatialindex](https://libspatialindex.github.io/)*
