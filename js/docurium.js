$(function() {
  // our document model - stores the datastructure generated from docurium
  var Docurium = Backbone.Model.extend({

    defaults: {'version': 'unknown'},

    initialize: function() {
      this.loadVersions()
    },

    loadVersions: function() {
      $.getJSON("project.json", function(data) {
        docurium.set({'versions': data.versions, 'github': data.github, 'signatures': data.signatures, 'name': data.name, 'groups': data.groups})
        if(data.name) {
          $('#site-title').text(data.name + ' API')
          document.title = data.name + ' API'
        }
        docurium.setVersionPicker()
        docurium.setVersion()
      })
    },

    setVersionPicker: function () {
      vers = docurium.get('versions')
      $('#version-list').empty().hide()
      for(var i in vers) {
        version = vers[i]
        vlink = $('<a>').attr('href', '#' + version).append(version).click( function() {
          $('#version-list').hide(100)
        })
        $('#version-list').append($('<li>').append(vlink))
      }
      vlink = $('<a>').attr('href', '#' + 'p/changelog').append("Changelog").click ( function () {
        $('#version-list').hide(100)
      })
      $('#version-list').append($('<li>').append(vlink))
    },

    setVersion: function (version) {
      if(!version) {
        version = _.first(docurium.get('versions'))
      }
      if(docurium.get('version') != version) {
        docurium.set({'version': version})
        $('#site-title').attr('href', '#' + version)
        docurium.loadDoc()
      }
    },

    loadDoc: function() {
      version = this.get('version')
      $.ajax({
        url: version + '.json',
        context: this,
        dataType: 'json',
        success: function(data){
          this.set({'data': data})
          Backbone.history.start()
        }
      })
    },

    collapseSection: function(data) {
      $(this).next().toggle(100)
      return false
    },

    showIndexPage: function() {
      version = docurium.get('version')
      ws.saveLocation(version)

      data = docurium.get('data')
      content = $('.content')
      content.empty()

      content.append($('<h1>').append("Public API Functions"))

      sigHist = docurium.get('signatures')

      // Function Groups
      for (var i in data['groups']) {
        group = data['groups'][i]
        content.append($('<h2>').addClass('funcGroup').append(group[0]))
        list = $('<p>').addClass('functionList')
        for(var j in group[1]) {
          fun = group[1][j]
          link = $('<a>').attr('href', '#' + groupLink(group[0], fun)).append(fun)
          if(sigHist[fun].changes[version]) {
            link.addClass('changed')
          }
          if(version == _.first(sigHist[fun].exists)) {
            link.addClass('introd')
          }
          list.append(link)
          if(j < group[1].length - 1) {
           list.append(', ')
          }
        }
        content.append(list)
      }
    },

    getGroup: function(gname) {
      var groups = docurium.get('data')['groups']
      for(var i in groups) {
        if(groups[i][0] == gname) {
          return groups[i]
        }
      }
    },

    showFun: function(gname, fname) {
      group = docurium.getGroup(gname)

      fdata = docurium.get('data')['functions']
      gname = group[0]
      functions = group[1]

      content = $('.content')
      content.empty()

      // Show Function Name
      content.append($('<h1>').addClass('funcTitle').append(fname))
      if(fdata[fname]['description']) {
        sub = content.append($('<h3>').addClass('funcDesc').append( ' ' + fdata[fname]['description'] ))
      }

      // Show Function Arguments
      argtable = $('<table>').addClass('funcTable')
      args = fdata[fname]['args']
      for(var i=0; i<args.length; i++) {
        arg = args[i]
        row = $('<tr>')
        row.append($('<td>').attr('valign', 'top').attr('nowrap', true).append(this.hotLink(arg.type)))
        row.append($('<td>').attr('valign', 'top').addClass('var').append(arg.name))
        row.append($('<td>').addClass('comment').append(arg.comment))
        argtable.append(row)
      }
      content.append(argtable)

      // Show Function Return Value
      retdiv = $('<div>').addClass('returns')
      retdiv.append($('<h3>').append("returns"))
      rettable = $('<table>').addClass('funcTable')
      retrow = $('<tr>')
      rettable.append(retrow)
      retdiv.append(rettable)

      ret = fdata[fname]['return']
      retrow.append($('<td>').attr('valign', 'top').append(this.hotLink(ret.type)))
      if(ret.comment) {
        retrow.append($('<td>').addClass('comment').append(ret.comment))
      }
      content.append(retdiv)

      // Show Non-Parsed Function Comments
      if (fdata[fname]['comments']) {
        content.append($('<pre>').append(fdata[fname]['comments']))
      }

      // Show Function Signature
      ex = $('<code>').addClass('params')
      ex.append(this.hotLink(fdata[fname]['return']['type'] + ' ' + fname + '(' + fdata[fname]['argline'] + ');'))
      example = $('<div>').addClass('example')
      example.append($('<h3>').append("signature"))
      example.append(ex)
      content.append(example)

      // Show Function History
      sigs = $('<div>').addClass('signatures')
      sigs.append($('<h3>').append("versions"))
      sigHist = docurium.get('signatures')[fname]
      for(var i in sigHist.exists) {
        ver = sigHist.exists[i]
        link = $('<a>').attr('href', '#' + groupLink(gname, fname, ver)).append(ver)
        if(sigHist.changes[ver]) {
          link.addClass('changed')
        }
        if(ver == docurium.get('version')) {
          link.addClass('current')
        }
        sigs.append(link)
      }
      content.append(sigs)

      // Link to Function Def on GitHub
      link = this.github_file(fdata[fname].file, fdata[fname].line, fdata[fname].lineto)
      flink = $('<a>').attr('target', 'github').attr('href', link).append(fdata[fname].file)
      content.append($('<div>').addClass('fileLink').append("Defined in: ").append(flink))

      // Show where this is used in the examples
      if(ex = fdata[fname].examples) {
        also = $('<div>').addClass('funcEx')
        also.append("Used in examples: ")
        for( fname in ex ) {
          lines = ex[fname]
          line = $('<li>')
          line.append($('<strong>').append(fname))
          for( var i in lines ) {
            flink = $('<a>').attr('href', lines[i]).append(' [' + (parseInt(i) + 1) + '] ')
            line.append(flink)
          }
          also.append(line)
        }
        content.append(also)
      }

      // Show other functions in this group
      also = $('<div>').addClass('also')
      flink = $('<a href="#' + docurium.get('version') + '/group/' + group[0] + '">' + group[0] + '</a>')
      flink.click( docurium.showGroup )
      also.append("Also in ")
      also.append(flink)
      also.append(" group: <br/>")

      for(i=0; i<functions.length; i++) {
        f = functions[i]
        d = fdata[f]
        link = $('<a>').attr('href', '#' + groupLink(gname, f)).append(f)
        also.append(link)
        also.append(', ')
      }
      content.append(also)


      this.addHotlinks()
    },

    showChangeLog: function() {
      content = $('.content')
      content.empty()
      content.append($('<h1>').append("Function Changelog"))
      // for every version, show which functions added, removed, changed - from HEAD down
      versions = docurium.get('versions')
      sigHist = docurium.get('signatures')

      lastVer = _.first(versions)

      // fill changelog struct
      changelog = {}
      for(var i in versions) {
        version = versions[i]
        changelog[version] = {'deletes': [], 'changes': [], 'adds': []}
      }

      // figure out the adds, deletes and changes
      for(var func in sigHist) {
        lastv = _.last(sigHist[func].exists)
        firstv = _.first(sigHist[func].exists)
        if (func != '__attribute__') {
          changelog[firstv]['adds'].push(func)
        }
        if(lastv && (lastv != lastVer)) {
          vi = _.indexOf(versions, lastv)
          delv = versions[vi - 1]
          changelog[delv]['deletes'].push(func)
        }
        for(var v in sigHist[func].changes) {
          changelog[v]['changes'].push(func)
        }
      }

      // display the data
      for(var i in versions) {
        version = versions[i]
        content.append($('<h3>').append(version))
        cl = $('<div>').addClass('changelog')

        console.log(version)

        for(var type in changelog[version]) {
          adds = changelog[version][type]
          adds.sort()
          addsection = $('<p>')
          for(var j in adds) {
            add = adds[j]
            if(type != 'deletes') {
              gname = docurium.groupOf(add)
              addlink = $('<a>').attr('href', '#' + groupLink(gname, add, version)).append(add)
            } else {
              addlink = add
            }
            addsection.append($('<li>').addClass(type).append(addlink))
          }
          cl.append(addsection)
        }
        content.append(cl)
      }
    },

    showType: function(data, manual) {
      if(manual) {
        id = '#typeItem' + domSafe(manual)
        ref = parseInt($(id).attr('ref'))
      } else {
        ref = parseInt($(this).attr('ref'))
      }
      tdata = docurium.get('data')['types'][ref]
      tname = tdata[0]
      data = tdata[1]

      ws.saveLocation(typeLink(tname))

      content = $('.content')
      content.empty()
      content.append($('<h1>').addClass('funcTitle').append(tname).append($("<small>").append(data.type)))

      content.append($('<p>').append(data.value))
      if(data.block) {
        content.append($('<pre>').append(data.block))
      }

      var ret = data.used.returns
      if (ret.length > 0) {
        content.append($('<h3>').append('Returns'))
      }
      for(var i=0; i<ret.length; i++) {
        gname = docurium.groupOf(ret[i])
        flink = $('<a>').attr('href', '#' + groupLink(gname, ret[i])).append(ret[i])
        flink.click( docurium.showFun )
        content.append(flink)
        content.append(', ')
      }

      var needs = data.used.needs
      if (needs.length > 0) {
        content.append($('<h3>').append('Argument In'))
      }
      for(var i=0; i<needs.length; i++) {
        gname = docurium.groupOf(needs[i])
        flink = $('<a>').attr('href', '#' + groupLink(gname, needs[i])).append(needs[i])
        flink.click( docurium.showFun )
        content.append(flink)
        content.append(', ')
      }

      link = docurium.github_file(data.file, data.line, data.lineto)
      flink = $('<a>').attr('target', 'github').attr('href', link).append(data.file)
      content.append($('<div>').addClass('fileLink').append("Defined in: ").append(flink))

      return false
    },

    showGroup: function(data, manual, flink) {
      if(manual) {
        id = '#groupItem' + manual
        ref = parseInt($(id).attr('ref'))
      } else {
        ref = parseInt($(this).attr('ref'))
      }
      group = docurium.get('data')['groups'][ref]
      fdata = docurium.get('data')['functions']
      gname = group[0]

      ws.saveLocation(groupLink(gname));

      functions = group[1]
      $('.content').empty()
      $('.content').append($('<h1>').append(gname + ' functions'))

      table = $('<table>').addClass('methods')
      for(i=0; i<functions.length; i++) {
        f = functions[i]
        d = fdata[f]
        row = $('<tr>')
        row.append($('<td>').attr('nowrap', true).attr('valign', 'top').append(d['return']['type'].substring(0, 20)))
        link = $('<a>').attr('href', '#' + groupLink(gname, f)).append(f)
        row.append($('<td>').attr('valign', 'top').addClass('methodName').append( link ))
        args = d['args']
        argtd = $('<td>')
        for(j=0; j<args.length; j++) {
          argtd.append(args[j].type + ' ' + args[j].name)
          argtd.append($('<br>'))
        }
        row.append(argtd)
        table.append(row)
      }
      $('.content').append(table)

      for(var i=0; i<functions.length; i++) {
        f = functions[i]
        argsText = '( ' + fdata[f]['argline'] + ' )'
        link = $('<a>').attr('href', '#' + groupLink(gname, f)).append(f)
        $('.content').append($('<h2>').append(link).append($('<small>').append(argsText)))
        $('.content').append($('<pre>').append(fdata[f]['rawComments']))
      }
      return false
    },

    // look for structs and link them 
    hotLink: function(text) {
      types = this.get('data')['types']
      for(var i=0; i<types.length; i++) {
        type = types[i]
        typeName = type[0]
        typeData = type[1]
        re = new RegExp(typeName + ' ', 'gi');
        link = '<a ref="' + i.toString() + '" class="typeLink' + domSafe(typeName) + '" href="#">' + typeName + '</a> '
        text = text.replace(re, link)
      }
      return text
    },

    groupOf: function (func) {
      return this.get('groups')[func]
    },

    addHotlinks: function() {
      types = this.get('data')['types']
      for(var i=0; i<types.length; i++) {
        type = types[i]
        typeName = type[0]
        className = '.typeLink' + domSafe(typeName)
        $(className).click( this.showType )
      }
    },

    refreshView: function() {
      data = this.get('data')

      // Function Groups
      menu = $('<li>')
      title = $('<h3><a href="#">Functions</a></h3>').click( this.collapseSection )
      menu.append(title)
      list = $('<ul>')
      _.each(data['groups'], function(group, i) {
        flink = $('<a href="#" ref="' + i.toString() + '" id="groupItem' + group[0] + '">' + group[0] + ' &nbsp;<small>(' + group[1].length + ')</small></a>')
        flink.click( this.showGroup )
        fitem = $('<li>')
        fitem.append(flink)
        list.append(fitem)
      }, this)
      menu.append(list)

      // Types
      title = $('<h3><a href="#">Types</a></h3>').click( this.collapseSection )
      menu.append(title)
      list = $('<ul>')

      fitem = $('<li>')
      fitem.append($('<span>').addClass('divide').append("Enums"))
      list.append(fitem)

      _.each(data['types'], function(group, i) {
        if(group[1]['block'] && group[1]['type'] == 'enum') {
          flink = $('<a href="#" ref="' + i.toString() + '" id="typeItem' + domSafe(group[0]) + '">' + group[0]  + '</a>')
          flink.click( this.showType )
          fitem = $('<li>')
          fitem.append(flink)
          list.append(fitem)
        }
      }, this)

      fitem = $('<li>')
      fitem.append($('<span>').addClass('divide').append("Structs"))
      list.append(fitem)

      _.each(data['types'], function(group, i) {
        if(group[1]['block'] && group[1]['type'] != 'enum') {
          flink = $('<a href="#" ref="' + i.toString() + '" id="typeItem' + domSafe(group[0]) + '">' + group[0]  + '</a>')
          flink.click( this.showType )
          fitem = $('<li>')
          fitem.append(flink)
          list.append(fitem)
        }
      }, this)

      fitem = $('<li>')
      fitem.append($('<span>').addClass('divide').append("Opaque Structs"))
      list.append(fitem)

      _.each(data['types'], function(group, i) {
        if(!group[1]['block']) {
          flink = $('<a href="#" ref="' + i.toString() + '" id="typeItem' + domSafe(group[0]) + '">' + group[0]  + '</a>')
          flink.click( this.showType )
          fitem = $('<li>')
          fitem.append(flink)
          list.append(fitem)
        }
      }, this)
      list.hide()
      menu.append(list)

      // File Listing
      title = $('<h3><a href="#">Files</a></h3>').click( this.collapseSection )
      menu.append(title)
      filelist = $('<ul>')
      _.each(data['files'], function(file) {
        url = this.github_file(file['file'])
        flink = $('<a target="github" href="' + url + '">' + file['file'] + '</a>')
        fitem = $('<li>')
        fitem.append(flink)
        filelist.append(fitem)
      }, this)
      filelist.hide()
      menu.append(filelist)

      // Examples List
      if(data['examples'] && (data['examples'].length > 0)) {
        title = $('<h3><a href="#">Examples</a></h3>').click( this.collapseSection )
        menu.append(title)
        filelist = $('<ul>')
        _.each(data['examples'], function(file) {
          fname = file[0]
          fpath = file[1]
          flink = $('<a>').attr('href', fpath).append(fname)
          fitem = $('<li>')
          fitem.append(flink)
          filelist.append(fitem)
        }, this)
        menu.append(filelist)
      }

      list = $('#files-list')
      list.empty()
      list.append(menu)
    },

    github_file: function(file, line, lineto) {
      url = "https://github.com/" + docurium.get('github')
      url += "/blob/" + docurium.get('version') + '/' + data.prefix + '/' + file
      if(line) {
        url += '#L' + line.toString()
        if(lineto) {
          url += '-' + lineto.toString()
        }
      } else {
        url += '#files'
      }
      return url
    },

    search: function(data) {
      var searchResults = []
      var value = $('#search-field').attr('value')
      if (value.length < 3) {
        return false
      }
      this.searchResults = []

      ws.saveLocation(searchLink(value))

      data = docurium.get('data')

      // look for functions (name, comment, argline)
      for (var name in data.functions) {
        f = data.functions[name]
        if (name.search(value) > -1) {
          gname = docurium.groupOf(name)
          var flink = $('<a>').attr('href', '#' + groupLink(gname, name)).append(name)
          searchResults.push(['fun-' + name, flink, 'function'])
        }
        if (f.argline) {
          if (f.argline.search(value) > -1) {
            gname = docurium.groupOf(name)
            var flink = $('<a>').attr('href', '#' + groupLink(gname, name)).append(name)
            searchResults.push(['fun-' + name, flink, f.argline])
          }
        }
      }
      for (var i in data.types) {
        var type = data.types[i]
        name = type[0]
        if (name.search(value) > -1) {
          var link = $('<a>').attr('href', '#' + typeLink(name)).append(name)
          searchResults.push(['type-' + name, link, type[1].type])
        }
      }

      // look for types
      // look for files
      content = $('.content')
      content.empty()

      content.append($('<h1>').append("Search Results"))
      table = $("<table>")
      var shown = {}
      for (var i in searchResults) {
        row = $("<tr>")
        result = searchResults[i]
        if (!shown[result[0]]) {
          link = result[1]
          match = result[2]
          row.append($('<td>').append(link))
          row.append($('<td>').append(match))
          table.append(row)
          shown[result[0]] = true
        }
      }
      content.append(table)

    }

  })

  var Workspace = Backbone.Controller.extend({

    routes: {
      "":                             "main",
      ":version":                     "main",
      ":version/group/:group":        "group",
      ":version/type/:type":          "showtype",
      ":version/group/:group/:func":  "groupFun",
      ":version/search/:query":       "search",
      "p/changelog":                  "changelog",
    },

    main: function(version) {
      docurium.setVersion(version)
      docurium.showIndexPage()
    },

    group: function(version, gname) {
      docurium.setVersion(version)
      docurium.showGroup(null, gname)
    },

    groupFun: function(version, gname, fname) {
      docurium.setVersion(version)
      docurium.showFun(gname, fname)
    },

    showtype: function(version, tname) {
      docurium.setVersion(version)
      docurium.showType(null, tname)
    },

    search: function(version, query) {
      docurium.setVersion(version)
      $('#search-field').attr('value', query)
      docurium.search()
    },

    changelog: function(version, tname) {
      docurium.setVersion()
      docurium.showChangeLog()
    },

  });

  function groupLink(gname, fname, version) {
    if(!version) {
      version = docurium.get('version')
    }
    if(fname) {
      return version + "/group/" + gname + '/' + fname
    } else {
      return version + "/group/" + gname
    }
  }

  function typeLink(tname) {
    return docurium.get('version') + "/type/" + tname
  }

  function searchLink(tname) {
    return docurium.get('version') + "/search/" + tname
  }

  function domSafe(str) {
    return str.replace('_', '-')
  }

    
  window.docurium = new Docurium
  window.ws = new Workspace

  docurium.bind('change:version', function(model, version) {
    $('#version').text(version)
  })
  docurium.bind('change:data', function(model, data) {
    model.refreshView()
  })

  $('#search-field').keyup( docurium.search )

  $('#version-picker').click( docurium.collapseSection )

})
