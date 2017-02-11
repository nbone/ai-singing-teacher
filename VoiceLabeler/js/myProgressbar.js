// requires jquery.ui
// requires lawnchair

// A jquery ui progressbar with automatic saved/remaining labels
function ProgressBar(jqProgressbarContainer, maxValue) {
	if (isNaN(maxValue) || maxValue <= 0) {
		throw "maxValue must be a positive integer";
	}
	
	// member variables
	this.jqContainer = jqProgressbarContainer;
	this.max = maxValue;
	this.saved = 0;
	this.remaining = maxValue;
	
	// extract title into label, if present
	var titleText = this.jqContainer.attr('title');
	if (titleText) {
		$('<label>').text(titleText).appendTo(this.jqContainer);
	}
	
	// init jquery progressbar
	this.jqProgressBar = $('<div></div>').progressbar({
		max: maxValue || 100,
		value: 0
	}).appendTo(this.jqContainer);
	
	// create labels
	var caption = $('<div class="caption"></div>');
	this.jqSaved = $('<span class="saved"></span>')
		.text('Saved: ' + this.saved)
		.appendTo(caption);
	this.jqRemaining = $('<span class="remaining"></span>')
		.text('Remaining: ' + this.remaining)
		.appendTo(caption);
	
	caption.appendTo(this.jqContainer);
}

ProgressBar.prototype.setValues = function(props) {
	if (props.saved !== undefined) {
		this.saved = props.saved;
		this.jqProgressBar.progressbar('value', this.saved);
		this.jqSaved.text('Saved: ' + this.saved);
	}
	
	this.remaining = this.max - this.saved;
	this.jqRemaining.text('Remaining: ' + this.remaining);
};


// A wrapper around ProgressBar that incorporates data persistence and file list management
function FileRatings(fileList, jqProgressbarContainer, metaDatabase, dbKey) {
	if (!fileList || !jqProgressbarContainer || !metaDatabase || !dbKey)
		throw 'Invalid arguments to FileRatings ctor!';
	
	var my = this;
	
	// init database to store file ratings
	this.ratingsData = new Lawnchair(
		{ name: dbKey, adapters: ['dom','webkit-sqlite','window-name'] },
		function(){});

	// set other member variables
	this.files = fileList;
	this.metaData = metaDatabase;
	this.key = dbKey;
	this.data = { key : dbKey, n : 0, k : 1, saved : 0 };
	this.jqElement = jqProgressbarContainer;
	this.progressbar = new ProgressBar(jqProgressbarContainer, fileList.length);
	
	// init
	this.init();
}

// load saved values from database, or init values and save to database
FileRatings.prototype.init = function() {
	var my = this;
	my.metaData.exists(my.key, function(exists) {
		// if data already exists use it; otherwise init all values
		if (exists) {
			my.metaData.get(my.key, function(record) {
				my.data = record;
			});
		} else {
			my.data.saved = 0;
			
			// start in a random place
			var fileListLength = my.files.length;
			my.data.n = Math.floor(Math.random() * fileListLength);
			// increment by a random value relatively prime to the length of the list
			my.data.k = 1;
			[2,3,5,7,11,13,17,19].forEach(function(p) {
				if (fileListLength % p != 0) {
					my.data.k *= Math.pow(p, Math.floor(Math.random() * 4));
					my.data.k %= fileListLength;
				}
			});
			
			//console.log(my.data);
			my.metaData.save(my.data);
		}
		
		// ensure progressbar displays correct values
		my.progressbar.setValues({ saved: my.data.saved });
	});
};

// get the associated jQuery element
FileRatings.prototype.getJQElement = function() {
	return this.jqElement;
};

// get all ratings data as a string
FileRatings.prototype.getAllRatings = function() {
	var my = this;
	var ratings;
	my.ratingsData.all(function(objs) {
		ratings = 'Ratings (' + my.key + ') : ' + objs.length + '\r\n' +
			objs.map(JSON.stringify).join('\r\n') + '\r\n';
	});
	return ratings;
};

// delete all ratings data and reset saved count to zero
FileRatings.prototype.nuke = function() {
	var my = this;
	my.data.saved = 0;
	my.metaData.save(my.data);
	my.ratingsData.nuke();
	my.progressbar.setValues({ saved: my.data.saved });
};

// save the given rating
FileRatings.prototype.save = function(data) {
	var my = this;
	my.ratingsData.exists(data.key, function(exists) {
		if (exists) {
			my.ratingsData.get(data.key, function(oldData) {
				console.log('Replacing ' + JSON.stringify(oldData) + ' with ' + JSON.stringify(data));
			});
		} else {
			my.data.saved++;
		}
		
		my.metaData.save(my.data);
		my.ratingsData.save(data);
		my.progressbar.setValues({ saved: my.data.saved });
	});
};

// get current file, and associated rating data (if any)
FileRatings.prototype.getCurrentFile = function() {
	var my = this;
	var file = my.files[my.data.n];
	console.log(my.data.n + ':' + file);
	
	var fileRating;
	my.ratingsData.get(file, function(obj) {
		fileRating = obj;
	});
	
	if (fileRating) {
		return fileRating;
	}
	
	return { key: file };
};

// advance to previous file, and return it
FileRatings.prototype.getPrevFile = function() {
	var my = this;
	my.data.n = (my.files.length + my.data.n - my.data.k) % my.files.length;
	my.metaData.save(my.data);
	return my.getCurrentFile();
};

// advance to next file, and return it
FileRatings.prototype.getNextFile = function() {
	var my = this;
	my.data.n = (my.data.n + my.data.k) % my.files.length;
	my.metaData.save(my.data);
	return my.getCurrentFile();
};

// advance to next UNRATED file, and return it; or false if all files are rated
FileRatings.prototype.getNextUnratedFile = function() {
	var my = this;
	var noMoreUnrated = false;
	
	my.ratingsData.keys(function(keys) {
		// if all files are rated already return false and skip the infinite loop
		if (keys.length >= my.files.length) {
			noMoreUnrated = true;
			return;
		}
		
		do {
			my.data.n = (my.data.n + my.data.k) % my.files.length;
		} while (keys.indexOf(my.files[my.data.n]) != -1);
	});
	
	if (noMoreUnrated) {
		return false;
	}
	
	my.metaData.save(my.data);
	
	return my.getCurrentFile();
};
