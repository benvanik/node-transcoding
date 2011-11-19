function declareProfile(key, name, options) {
  var profile = {
    name: name,
    options: options
  };
  exports[key] = profile;
}

declareProfile('APPLE_TV_2', 'Apple TV 2', {

});

declareProfile('PLAYSTATION_3', 'Playstation 3', {

});

declareProfile('XBOX_360', 'Xbox 360', {

});
