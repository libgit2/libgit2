// Init sidebar
$(function() {
  var activeItem,
      helpList = $('#js-help-sidebar .subgroup'),
      firstOccurance = true

  $('#js-help-sidebar .subgroup').each(function(){
    if($(this).find('.disable').length == 0 || firstOccurance == false){
      $(this).find('.js-guides').children().hide()
    } else {
      activeItem = $(this).index()
      firstOccurance = false
    }
  })

  $('#js-help-sidebar .subgroup h3 a').click(function(){
    var clickedTopic = $(this).parents('.subgroup'),
        topicGuides = clickedTopic.find('.subgroup li')
    
    if(activeItem != clickedTopic.index()){
      if(helpList.eq(activeItem)){
        helpList.eq(activeItem).find('.subgroup li').toggle(100)
      }
      activeItem = clickedTopic.index()
      topicGuides.toggle(100)
    } else {
      activeItem = undefined
      topicGuides.toggle(100)
    }

    return false
  })

  $('.help-search .search-box').focus(function(){
    $(this).css('background-position','0px -25px')
  })

  $('.help-search .search-box').focusout(function(){
    if($(this).val() == ''){
      $(this).css('background-position','0px 0px')
    }
  })
});
