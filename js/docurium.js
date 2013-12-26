$(function() {
  // our document model - stores the datastructure generated from docurium
  var Docurium = Backbone.Model.extend({

    defaults: {'version': 'unknown'},

    initialize: function() {
      this.loadVersions()
    },

    loadVersions: function() {
      $.getJSON("project.json").then(function(data) {
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
      hideVersionList = function() { $('#version-list').hide(100) }
      vers = docurium.get('versions')
      template = _.template($('#version-picker-template').html())
      // make sure this is a jquery object so we can use click()
      list = $(template({versions: vers})).hide()
      $('a', list).click(hideVersionList)
      $('#version-list').replaceWith(list)
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
      $.getJSON(version + '.json').then(function(data) {
	// use data as a proxy for whether we've started the history
	hadData = docurium.get('data') != undefined
        docurium.set({'data': data})
	if (!hadData)
	  Backbone.history.start()
      })
    },

    collapseSection: function(data) {
      $(this).next().toggle(100)
      return false
    },

    showIndexPage: function(replace) {
      version = docurium.get('version')
      ws.navigate(version, {replace: replace})

      data = docurium.get('data')
      content = $('<div>').addClass('content')
      content.append($('<h1>').append("Public API Functions"))

      sigHist = docurium.get('signatures')

      // Function Group
      data.groups.forEach(function(group) {
        content.append($('<h2>').addClass('funcGroup').append(group[0]))
        list = $('<p>').addClass('functionList')
	links = group[1].map(function(fun) {
          link = $('<a>').attr('href', '#' + groupLink(group[0], fun)).append(fun)
          if(sigHist[fun].changes[version]) {
            link.addClass('changed')
          }
          if(version == _.first(sigHist[fun].exists)) {
            link.addClass('introd')
          }
	  return link
	})

	// intersperse commas between each function
	for(var i = 0; i < links.length - 1; i++) {
	  list.append(links[i])
	  list.append(", ")
	}
	list.append(_.last(links))

	content.append(list)
      })

      $('.content').replaceWith(content)
    },

    getGroup: function(gname) {
      var groups = docurium.get('data')['groups']
      return _.find(groups, function(g) {
	return g[0] == gname
      })
    },

    showFun: function(gname, fname) {
      group = docurium.getGroup(gname)

      fdata = docurium.get('data')['functions']
      gname = group[0]
      functions = group[1]

      document.body.scrollTop = document.documentElement.scrollTop = 0;
      content = $('<div>').addClass('content')

      // Show Function Name
      content.append($('<h1>').addClass('funcTitle').append(fname))
      if(fdata[fname]['description']) {
        sub = content.append($('<h3>').addClass('funcDesc').append( ' ' + fdata[fname]['description'] ))
      }

      // Show Function Arguments
      argtable = $('<table>').addClass('funcTable')
      fdata[fname]['args'].forEach(function(arg) {
        row = $('<tr>')
        row.append($('<td>').attr('valign', 'top').attr('nowrap', true).append(this.hotLink(arg.type)))
        row.append($('<td>').attr('valign', 'top').addClass('var').append(arg.name))
        row.append($('<td>').addClass('comment').append(arg.comment))
        argtable.append(row)
      }, this)
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
      if (fdata[fname]['comments'])
        content.append($('<div>').append(fdata[fname]['comments']))

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
      var list = $('<ul>')
      for(var i in sigHist.exists) {
        ver = sigHist.exists[i]
        link = $('<a>').attr('href', '#' + groupLink(gname, fname, ver)).append(ver)
        if(sigHist.changes[ver]) {
          link.addClass('changed')
        }
        if(ver == docurium.get('version')) {
          link.addClass('current')
        }
        list.append($('<li>').append(link))
      }
      sigs.append(list)
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
      flink = $('<a>')
	.attr('href', '#' + docurium.get('version') + '/group'/ + group[0])
	.append(group[0])
      flink.click(docurium.showGroup)

      also.append("Also in ")
      also.append(flink)
      also.append(" group: <br/>")

      links = _.map(functions, function(f) {
        return $('<a>').attr('href', '#' + groupLink(gname, f)).append(f)
      })
      for (i = 0; i < links.length-1; i++) {
	also.append(links[i])
        also.append(', ')
      }
      also.append(_.last(links))
      content.append(also)

      $('.content').replaceWith(content)
      this.addHotlinks()
    },

    showChangeLog: function() {
      template = _.template($('#changelog-template').html())
      itemTemplate = _.template($('#changelog-item-template').html())

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
      _.forEach(sigHist, function(func, fname) {
	lastv = _.last(func.exists)
	firstv = _.first(func.exists)
	changelog[firstv]['adds'].push(fname)

	// figure out where it was deleted or changed
	if (lastv && (lastv != lastVer)) {
	  vi = _.indexOf(versions,lastv)
	  delv = versions[vi-1]
	  changelog[delv]['deletes'].push(fname)

	  _.forEach(func.changes, function(_, v) {
	    changelog[v]['changes'].push(fname)
	  })
	}
      })

      vers = _.map(versions, function(version) {
	deletes = changelog[version]['deletes']
	deletes.sort()

	additions = changelog[version]['adds']
	additions.sort()
	adds = _.map(additions, function(add) {
          gname = docurium.groupOf(add)
	  return {link: groupLink(gname, add, version), text: add}
	})

	return {title: version, listing: itemTemplate({dels: deletes, adds: adds})}
      })

      $('.content').html(template({versions: vers}))
    },

    showType: function(data, manual) {
      if(manual) {
        id = '#typeItem' + manual
        ref = parseInt($(id).attr('ref'))
      } else {
        ref = parseInt($(this).attr('ref'))
      }
      tdata = docurium.get('data')['types'][ref]
      tname = tdata[0]
      data = tdata[1]

      ws.navigate(typeLink(tname))
      document.body.scrollTop = document.documentElement.scrollTop = 0;

      content = $('<div>').addClass('content')
      content.append($('<h1>').addClass('funcTitle').append(tname).append($("<small>").append(data.type)))

      content.append($('<p>').append(data.value))

      if(data.comments) {
	content.append($('<div>').append(data.comments))
      }

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

      $('.content').replaceWith(content)
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

      ws.navigate(groupLink(gname));
      document.body.scrollTop = document.documentElement.scrollTop = 0;

      functions = group[1]
      content = $('<div>').addClass('content')
      content.append($('<h1>').append(gname + ' functions'))

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
      content.append(table)

      for(var i=0; i<functions.length; i++) {
        f = functions[i]
        argsText = '( ' + fdata[f]['argline'] + ' )'
        link = $('<a>').attr('href', '#' + groupLink(gname, f)).append(f)
        content.append($('<h2>').append(link).append($('<small>').append(argsText)))
        description = fdata[f]['description']
	if(fdata[f]['comments'])
		description += "\n\n" + fdata[f]['comments']

	content.append($('<div>').addClass('description').append(description))
      }

      $('.content').replaceWith(content)
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
        link = '<a ref="' + i.toString() + '" class="typeLink' + typeName + '" href="#">' + typeName + '</a> '
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
        className = '.typeLink' + typeName
        $(className).click( this.showType )
      }
    },

    refreshView: function() {
      template = _.template($('#file-list-template').html())
      data = this.get('data')

      // Function groups
      funs = _.map(data['groups'], function(group, i) {
	return {name: group[0], num: group[1].length}
      })

      // Types
      var getName = function(type) {
	return {ref: type.ref, name: type.type[0]}
      }

      // We need to keep the original index around in order to show
      // the right one when clicking on the link
      var types = _.map(data['types'], function(type, i) {
	return {ref: i, type: type}
      })

      enums = types.filter(function(type) {
	return type.type[1]['block'] && type.type[1]['type'] == 'enum';
      }).map(getName)

      structs = types.filter(function(type) {
	return type.type[1]['block'] && type.type[1]['type'] != 'enum'
      }).map(getName)

      opaques = types.filter(function(type) {
	return !type.type[1]['block']
      }).map(getName)

      // File Listing
      files = _.map(data['files'], function(file) {
	url = this.github_file(file['file'])
	return {url: url, name: file['file']}
      }, this)

      // Examples List
      examples = []
      if(data['examples'] && (data['examples'].length > 0)) {
	examples = _.map(data['examples'], function(file) {
	  return {name: file[0], path: file[1]}
	})
      }

      menu = $(template({funs: funs, enums: enums, structs: structs, opaques: opaques,
			 files: files, examples: examples}))

      $('a.group', menu).click(this.showGroup)
      $('a.type', menu).click(this.showType)
      $('h3', menu).click(this.collapseSection)
     
      $('#files-list').html(menu)
    },

    github_file: function(file, line, lineto) {
      url = ['https://github.com', docurium.get('github'),
	     'blob', docurium.get('version'), data.prefix, file].join('/')
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
      var value = $('#search-field').val()

      if (value.length < 3) {
        docurium.showIndexPage(false)
        return
      }

      this.searchResults = []

      ws.navigate(searchLink(value))

      data = docurium.get('data')

      // look for functions (name, comment, argline)
      _.forEach(data.functions, function(f, name) {
	gname = docurium.groupOf(name)
	// look in the function name first
        if (name.search(value) > -1) {
          var flink = $('<a>').attr('href', '#' + groupLink(gname, name)).append(name)
	  searchResults.push({link: flink, match: 'function', navigate: groupLink(gname, name)})
	  return
        }

	// if we didn't find it there, let's look in the argline
        if (f.argline && f.argline.search(value) > -1) {
          var flink = $('<a>').attr('href', '#' + groupLink(gname, name)).append(name)
          searchResults.push({link: flink, match: f.argline, navigate: groupLink(gname, name)})
        }
      })

      // look for types
      data.types.forEach(function(type) {
        name = type[0]
        if (name.search(value) > -1) {
          var link = $('<a>').attr('href', '#' + typeLink(name)).append(name)
          searchResults.push({link: link, match: type[1].type, navigate: typeLink(name)})
        }
      })

      // if we have a single result, show that page
      if (searchResults.length == 1) {
         ws.navigate(searchResults[0].navigate, {trigger: true, replace: true})
         return
      }

      content = $('<div>').addClass('content')
      content.append($('<h1>').append("Search Results"))
      rows = _.map(searchResults, function(result) {
	return $('<tr>').append(
	  $('<td>').append(result.link),
	  $('<td>').append(result.match))
      })

      content.append($('<table>').append(rows))
      $('.content').replaceWith(content)
    }

  })

  var Workspace = Backbone.Router.extend({

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
      // when asking for '/', replace with 'HEAD' instead of redirecting
      var replace = version == undefined
      docurium.showIndexPage(replace)
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
      $('#search-field').val(query)
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
