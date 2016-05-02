/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
var chai = require('chai'),
  expect = chai.expect,
  assert = chai.assert,
  sidx = require('bindings')('spatialindex');

describe("SpatialIndex tests", function() {
  var index;
  var mins = [0.0, 1.0];
  var maxs = [0.0, 1.0];
  var bounds = [0, 1, 0, 1];
  var buf = new Buffer('POINT(0 1)');
  var version = '1.8.5';

  beforeEach(function(done) {
    index = new sidx.SpatialIndex();
    index.open(function(err, res){
      if (err)
        done(err);
      else
        done();
    });
  });

  afterEach(function(){
  })

  describe("libspatialindex tests", function(done) {
    it("Test options constructor", function(){
      var index2 = new sidx.SpatialIndex({
        "type": "rtree",
        "storage": "memory",
        "dimension": 3
      });
      index2.open(function(err, res){
          if (err)
            done(err);
          else{
            expect(index2.dimension()).to.equal(3);
            done();
          }
      });
    }),

    it("Test version string", function(done){
      index.version(function(err, result){
        if (err){
          done(err);
        } else {
          expect(result).to.equal(version);
          done();
        }
      })
    });

    it("Test insert and retrieve data", function(done){
      // note, use WKB in prod for for efficient storage
      index.insert(1, mins, maxs, buf,
        function(err, result){
          if (err){
            done(err);
          } else{
            // result is an array of {id: ..., data: ...}
            index.intersects(mins, maxs, function(err, result){
              if (err){
                done(err);
              } else {
                expect(result.length).to.equal(1);
                var obj = result[0];
                expect(obj.id).to.equal(1);
                expect(obj.data.toString('ascii')).to.equal(buf.toString());
                done();
              }
            });
          }
        });
    });

    it("Test bounds", function(done){
      index.insert(1, mins, maxs, buf,
        function(err, result){
          if (err){
            done(err);
          } else{
            index.bounds(function(err, result){
              expect(result.length).to.equal(bounds.length);
              expect(result[0]).to.equal(bounds[0]);
              expect(result[1]).to.equal(bounds[1]);
              expect(result[2]).to.equal(bounds[2]);
              expect(result[3]).to.equal(bounds[3]);
              done();
            });
          }
      })
    });

    it ("Test delete data", function(done){
      index.insert(1, mins, maxs, buf, function(err, result){
        if (err){
          done(err);
        } else {
          index.delete(1, mins, maxs, function(err, result){
            if (err){
              done(err);
            } else {
              done();  
            }
          });
        }
      })
    });

    it ("Test offset and limit data", function(done){
      var cntr = 0;
      var max = 10;
      // note use Promise libraries rather than counters in prod for clarity
      // don't want to add a bluebird or other dependency to this library for testing
      cb = function(err, result){
        if (err){
          done(err);
        } else{
          if (++cntr == max){
            index.intersects([0, 0], [10, 10], 4, 7, function(err, result){
              if (err){
                done(err);
              } else{
                expect(result.length).to.equal(6);
                expect(result[0].id).to.equal(4);
                expect(result[0].data.toString()).to.equal('POINT(4 4)')
                done();
              }
            });
          }
        }
      }
      for (var i = 0; i < max; i++){
        var Pt = 'POINT(' + i + ' ' + i + ')'
        index.insert(i, [i, i],[i, i], new Buffer(Pt), cb);
      }
    });
  });
});
