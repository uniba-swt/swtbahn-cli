var driver = null;           // Train driver logic.
var drivingTimer = null;     // Timer for driving a train.
var serverAddress = "";      // The base address of the server.
var language = "";           // User interface language.
var isEasyMode = false;      // User interface verbosity.



/**************************************************
 * Destination related information/functionality
 */

const numberOfDestinationsMax = 12;           // Maximum destinations to display
const destinationNamePrefix = "destination";  // HTML element ID prefix of the destination buttons

// Initialisation of Lookup tables
var allPossibleDestinations = null;  // Global Access variable for destination lookup table
var signalFlagMap = null; // Global access variable for signal flag mapping

// Tries to determine the name of the SWTbahn Platform which is being used by querying the server.
// If the server does not reply with the platform name, retry after some delay.
// If the server does reply with the platform name, tries to load the destinations and the flags 
// mapped for the respective platform. If this fails, does not perform a retry.
function tryLoadDestinationsAndFlagMap() {
	let retryDelay = 2000;
	fetch(serverAddress + '/monitor/platform-name')
		.then(response => {
			if (response.status !== 200) {
				console.log("Getting platform name failed: Retrying in %d ms", retryDelay);
				setTimeout(() => tryLoadDestinationsAndFlagMap(), retryDelay);
			} else {
				return response.json();
			}
		})
		.then(data => {
			if (data !== undefined && data["platform-name"]) {
				console.log("Platform name received from server:", data["platform-name"]);
				let platform = data["platform-name"].toLowerCase();
				allPossibleDestinations = eval("allPossibleDestinations_" + platform);
				signalFlagMap = eval("signalFlagMap_" + platform);
			}
		})
		.catch(error => {
			console.error("tryLoadDestinationsAndFlagMap (get platform name) err:", error);
		});
}

// Returns the destinations possible from a given block
function getDestinations(blockId) {
	if (allPossibleDestinations == null || !allPossibleDestinations.hasOwnProperty(blockId)) {
		return null;
	}
	return allPossibleDestinations[blockId];
}

function disableAllDestinationButtons() {
	driver.clearUpdatePossibleDestinationsInterval();

	for (let i = 0; i < numberOfDestinationsMax; i++) {
		$(`#${destinationNamePrefix}${i}`).val("");
		$(`#${destinationNamePrefix}${i}`).attr("class", "flagThemeBlank");
	}
}

function setDestinationButton(routeIndex, route, destinationSignal) {
	// "Clean" the "a" or "b" suffix of composite signal if present
	const destinationSigCleaned = destinationSignal.replace(/(a|b)$/, '');
	// Route details are stored in the value parameter of the destination button
	$(`#${destinationNamePrefix}${routeIndex}`).val(JSON.stringify(route));
	$(`#${destinationNamePrefix}${routeIndex}`).attr("class", signalFlagMap[destinationSigCleaned]);
}

function setDestinationButtonAvailable(routeIndex, route, destinationSignal) {
	setDestinationButton(routeIndex, route, destinationSignal);
	$(`#${destinationNamePrefix}${routeIndex}`).removeClass("flagThemeDisabled");
}

function setDestinationButtonUnavailable(routeIndex, route, destinationSignal) {
	setDestinationButton(routeIndex, route, destinationSignal);
	$(`#${destinationNamePrefix}${routeIndex}`).addClass("flagThemeDisabled");
}

// Periodically update the availability of a blocks possible destinations.
// Update can be stopped by cancelling updatePossibleDestinationsInterval,
// e.g., when disabling the destination buttons or ending the game.
function updatePossibleDestinations(blockId) {
	// Show the form that contains all the destination buttons
	$('#destinationsFormContainer').show();
	disableAllDestinationButtons();

	// Set up a timer interval to periodically update the availability
	const updatePossibleDestinationsTimeout = 1000; // 1000ms
	driver.updatePossibleDestinationsInterval = setInterval(() => {
		const routesFromCurrentBlock = getDestinations(blockId);
		if (routesFromCurrentBlock == null) {
			console.warn('updatePossibleDestinations: routesFromCurrentBlock is NULL');
			return;
		}

		// Object.keys foreach 
		// -> routeIndex is the index of the route with destination signal `destinationSignal`
		//    in the collection `routesFromCurrentBlock`
		Object.keys(routesFromCurrentBlock).forEach((destinationSignal, routeIndex) => {
			let route = {};
			route[destinationSignal] = routesFromCurrentBlock[destinationSignal];
			route['destSignalID'] = destinationSignal;
			const routeId = route[destinationSignal]["route-id"];
			updateDestinationAvailabilityPromise(
				routeId,
				// route is available
				() => setDestinationButtonAvailable(routeIndex, route, destinationSignal),
				// route is unavailable
				() => setDestinationButtonUnavailable(routeIndex, route, destinationSignal)
			);
		});
	}, updatePossibleDestinationsTimeout);
}

// Request for a route's status and then determine whether the route is available
function updateDestinationAvailabilityPromise(routeId, available, unavailable) {
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/monitor/route',
		crossDomain: true,
		data: { 'route-id': routeId },
		dataType: 'text',
		success: (responseText) => {
			const responseJson = JSON.parse(responseText);
			const noConflicts = Array.isArray(responseJson.granted_conflicting_route_ids) &&
				responseJson.granted_conflicting_route_ids.length === 0;
			const isClear = responseJson.clear === true;
			const isNotGranted = responseJson.granted_to_train === "";
			const isAvailable = noConflicts && isClear && isNotGranted;

			if (isAvailable) {
				available();
			} else {
				unavailable();
			}
		},
		error: (jqXHR) => {
			if ('msg' in jqXHR.responseText) {
				const responseJson = JSON.parse(responseText);
				console.warn("/monitor/route error with returned message:", responseJson.msg);
			} else {
				console.warn("/monitor/route error with no message, status:", jqXHR.status);
			}
			unavailable();
			setResponseDanger(
				'#serverResponse',
				'😢 There was a problem checking the destinations',
				'😢 Es ist ein Problem beim Überprüfen der Ziele aufgetreten',
				'Sorry'
			);
		}
	});
}

function clearChosenDestination() {
	$('#destination').attr("class", "flagThemeBlank");
}

function setChosenDestination(destination) {
	destination = destination.replace(/(a|b)$/, '');
	$('#destination').attr("class", signalFlagMap[destination]);
}

function setChosenTrain(trainId) {
	const imageName = `img/train-${trainId.replace("_", "-")}.jpg`
	$("#chosenTrain").attr("src", imageName);
}

/**************************************************
 * Train speed UI elements/functionality
 */

const speedButtons = [
	"stop",
	"slow",
	"normal",
	"fast"
];

/* Per-train speed factors -> necessary as a dcc speed setting means different
   real-world speeds for different trains. "Slow" being dcc speed 20 causes the slower
   trains to get stuck as they are not fast enough to get through double-slip switches.
   Ideally the server would have this in the config, but that'd be a lot of work to 
   implement/add, so for this client it's just defined here.
*/ 
const trainSpeedFactors = new Map([
	["cargo_db", 1.0],
	["cargo_bayern", 1.2],
	["cargo_green", 1.1],
	["cargo_g1000bb", 2.0],
	["perso_sbb", 0.85],
	["regional_odeg", 2.0],
	["regional_brengdirect", 2.0],
])

function disableSpeedButtons() {
	$('#drivingForm').hide();
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', true);
	});
	drivingTimer.stop();
}

function enableSpeedButtons(destination) {
	$('#drivingForm').show();
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', false);
	});
	drivingTimer.start();
}



/**************************************************
 * Feedback UI elements
 */
var responseTimer = null;

function setResponse(responseId, messageEn, messageDe, callback) {
	clearTimeout(responseTimer);

	const message = (language == 'en') ? messageEn : messageDe;
	$(responseId).text(message);
	callback();

	$(responseId).parent().fadeIn("fast");
	const responseTimeout = 7000;
	responseTimer = setTimeout(() => {
		$(responseId).parent().fadeOut("slow");
	}, responseTimeout);
}

function speak(text) {
	if (!text) {
		// Don't speak if text is not truthy, i.e., undefined, null, or empty.
		return;
	}
	let speakMsg = new SpeechSynthesisUtterance(text);
	///NOTE: At the moment, all voice messages are in german or "german-ish" (like "Sorry")
	//speakMsg.lang = (language == 'en') ? "en-US" : "de-DE";
	speakMsg.lang = "de-DE";
	for (const voice of window.speechSynthesis.getVoices()) {
		if (voice.lang == "de-DE") {
			speakMsg.voice = voice;
			break;
		}
	}
	window.speechSynthesis.speak(speakMsg);
}

function setResponseDanger(responseId, messageEn, messageDe, messageSpeak) {
	setResponse(responseId, messageEn, messageDe, function () {
		$(responseId).parent().addClass('alert-danger');
		$(responseId).parent().addClass('alert-danger-blink');
		$(responseId).parent().removeClass('alert-success');
	});
	speak(messageSpeak);
}

function setResponseSuccess(responseId, messageEn, messageDe, messageSpeak) {
	setResponse(responseId, messageEn, messageDe, function () {
		$(responseId).parent().removeClass('alert-danger');
		$(responseId).parent().removeClass('alert-danger-blink');
		$(responseId).parent().addClass('alert-success');
	});
	speak(messageSpeak);
}

const modalMessages = {
	drivingInfringement: {
		title: {
			de: 'Unsichere Fahrt! 👎',
			en: 'Driving Infringement! 👎'
		},
		body: {
			de: 'Du hast deinen Zug nicht vor dem Zielsignal gestoppt! <br/><br/> <svg class="flag" viewBox="0 0 100 100"><use href="#flagDef" class="${destination}"></use></svg> <br/><br/> Glücklicherweise konnten wir deinen Zug stoppen, bevor dieser mit einem anderen kollidieren oder die Schienen beschädigen konnte.',
			en: 'You did not stop your train before the destination signal! <br/><br/> <svg class="flag" viewBox="0 0 100 100"><use href="#flagDef" class="${destination}"></use></svg> <br/><br/> Luckily, we were able to stop your train before it crashed into another train or damaged the tracks.'
		},
		button: {
			de: 'Verstanden',
			en: 'Understood'
		}
	},
	drivingContinue: {
		title: {
			de: 'Fahr weiter',
			en: 'Continue Driving'
		},
		body: {
			de: 'Du hast dein Ziel noch nicht erreicht. Bitte fahr weiter.',
			en: 'You have not yet reached your destination. Please continue driving.'
		},
		button: {
			de: 'Verstanden',
			en: 'Understood'
		}
	},
	drivingSuccess: {
		title: {
			de: 'Ziel erreicht!',
			en: 'Destination Reached!'
		},
		body: {
			de: 'Du hast deinen Zug zur deiner ausgewählten Station gefahren! 🥳',
			en: 'You drove your train to your chosen destination! 🥳'
		},
		button: {
			de: 'Super!',
			en: 'Awesome!'
		}
	}
};

function setModal(message) {
	$('#serverModal .modal-content').removeClass('modal-danger');
	$('#serverModal .modal-content').removeClass('modal-success');

	const title = (language == 'en') ? message.title.en : message.title.de;
	const body = (language == 'en') ? message.body.en : message.body.de;
	const button = (language == 'en') ? message.button.en : message.button.de;

	$('#serverModalTitle').text(title);
	$('#serverModalBody').html(body);
	$('#serverModalButton').text(button);

	let modalElement = document.getElementById('serverModal');
	let modal = bootstrap.Modal.getOrCreateInstance(modalElement);
	modal.show();
}

function setModalDanger(message, messageSpeak) {
	setModal(message);
	$('#serverModal .modal-content').addClass('modal-danger');
	speak(messageSpeak);
}

function setModalSuccess(message, messageSpeak) {
	setModal(message);
	$('#serverModal .modal-content').addClass('modal-success');
	speak(messageSpeak);
}

/**************************************************
 * Driver class that controls the driving of a
 * train and the UI elements
 */

class Driver {
	// Server and train details
	sessionId = null;
	trackOutput = null;
	trainEngine = null;
	trainId = null;
	grabId = null;

	// Driving details
	routeDetails = null;
	drivingIsForwards = null;
	currentBlock = null;
	isDestinationReached = null;

	// Timer intervals
	trainAvailabilityInterval = null;
	updatePossibleDestinationsInterval = null;
	destinationReachedInterval = null;


	constructor(trackOutput, trainEngine, trainId) {
		this.sessionId = 0;
		this.trackOutput = trackOutput;
		this.trainEngine = trainEngine;
		this.trainId = trainId;
		this.grabId = -1;

		this.routeDetails = null;
		this.drivingIsForwards = null;
		this.currentBlock = null;
		this.isDestinationReached = false;

		this.trainAvailabilityInterval = null;
		this.updatePossibleDestinationsInterval = null;
		this.destinationReachedInterval = null;

		drivingTimer = new Timer();
	}

	resetTrainSession() {
		this.sessionId = 0;
		this.grabId = -1;
	}

	hasValidTrainSession() {
		return (this.sessionId != 0 && this.grabId != -1)
	}

	hasRouteGranted() {
		return (this.routeDetails != null);
	}

	clearTrainAvailabilityInterval() {
		console.log("clearTrainAvailabilityInterval");
		clearInterval(this.trainAvailabilityInterval);
	}

	clearUpdatePossibleDestinationsInterval() {
		console.log("clearUpdatePossibleDestinationsInterval");
		clearInterval(this.updatePossibleDestinationsInterval);
	}

	clearDestinationReachedInterval() {
		console.log("clearDestinationReachedInterval");
		clearInterval(this.destinationReachedInterval);
	}

	// Request for the train's current block
	updateCurrentBlockPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/monitor/train-state',
			crossDomain: true,
			data: { 'train': this.trainId },
			dataType: 'text',
			success: (responseText) => {
				const responseJson = JSON.parse(responseText);
				if (responseJson.on_track && Array.isArray(responseJson.occupied_blocks) 
					&& responseJson.occupied_blocks.length > 0) {
					this.currentBlock = responseJson.occupied_blocks[0];
				}
			},
			error: (jqXHR) => {
				if ('msg' in jqXHR.responseText) {
					const responseJson = JSON.parse(responseText);
					console.warn("/monitor/train-state error with returned message:", responseJson.msg);
				} else {
					console.warn("/monitor/train-state error with no message, status:", jqXHR.status);
				}
				setResponseDanger('#serverResponse',
					'Could not find your train 😢',
					'Dein Zug konnte nicht gefunden werden 😢',
					''
				);
			}
		});
	}


	// Request for a train's status and then execute the callback handlers
	trainIsAvailablePromise(trainId, successCallback, errorCallback) {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/monitor/train-state',
			crossDomain: true,
			data: { 'train': trainId },
			dataType: 'text',
			success: (responseText) => {
				const responseJson = JSON.parse(responseText);
				if (responseJson.grabbed === false && responseJson.on_track === true) {
					successCallback();
				} else {
					errorCallback();
				}
			},
			error: (jqXHR) => {
				if ('msg' in jqXHR.responseText) {
					const responseJson = JSON.parse(responseText);
					console.warn("/monitor/train-state error with returned message:", responseJson.msg);
				} else {
					console.warn("/monitor/train-state error with no message, status:", jqXHR.status);
				}
				errorCallback();
			}
		});
	}


	// Update the styling of the train selection buttons based on the train availabilities
	updateTrainAvailability() {
		$('.selectTrainButton').prop("disabled", true);

		const trainAvailabilityTimeout = 1000;
		this.trainAvailabilityInterval = setInterval(() => {

			// Enable a train if it is on the tracks and has not been grabbed
			$('.selectTrainButton').each((index, obj) => {
				let trainId = obj.id;
				this.trainIsAvailablePromise(
					trainId,
					// Success Callback
					() => {
						$(obj).prop("disabled", false);
						$(obj).removeClass("btn-danger");
						$(obj).addClass("btn-primary");
						$($(obj).parent(".card-body").parent(".card")).removeClass("unavailableTrain");

						function setVisibility(isShow) {
							if (isShow) {
								return "";
							} else {
								return "style='display: none;'";
							}
						}
						$(obj).children().each(function () {
							switch ($(this).attr('lang')) {
								case "de": $(this).html(`<span class='easy' ${setVisibility(isEasyMode)}>Klicke um den Zug zu fahren</span><span class='normal' ${setVisibility(!isEasyMode)}>Zug fahren</span>`);
									break;
								case "en": $(this).html(`<span class='easy' ${setVisibility(isEasyMode)}>Click to drive this train</span><span class='normal' ${setVisibility(!isEasyMode)}>Drive this train</span>`);
									break;
							}
						});
					},
					// Error Callback
					() => {
						$(obj).prop("disabled", true);
						$(obj).removeClass("btn-primary");
						$(obj).addClass("btn-danger");
						$($(obj).parent(".card-body").parent(".card")).addClass("unavailableTrain");
						$(obj).children().each(function () {
							switch ($(this).attr('lang')) {
								case "de": $(this).text("Nicht verfügbar"); break;
								case "en": $(this).text("Unavailable"); break;
							}
						});
					}
				);
			})
		}, trainAvailabilityTimeout);
	}


	// Request to grab a train
	grabTrainPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/driver/grab-train',
			crossDomain: true,
			data: {
				'train': this.trainId,
				'engine': this.trainEngine
			},
			dataType: 'text',
			success: (responseText) => {
				const responseJson = JSON.parse(responseText);
				this.sessionId = responseJson['session-id'];
				this.grabId = responseJson['grab-id'];
				setResponseSuccess('#serverResponse', 
					'Your train is ready 😁', 
					'Dein Zug ist bereit 😁', 
					''
				);
			},
			error: (jqXHR) => {
				if ('msg' in jqXHR.responseText) {
					const responseJson = JSON.parse(responseText);
					console.warn("/driver/grab-train error with returned message:", responseJson.msg);
				} else {
					console.warn("/driver/grab-train error with no message, status:", jqXHR.status);
				}
				setResponseDanger('#serverResponse',
					'There was a problem starting your train 😢',
					'Es ist ein Problem beim Starten deines Zuges aufgetreten 😢',
					''
				);
			}
		});
	}


	// Request for the train's physical driving direction of its granted route
	updateDrivingDirectionPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/driver/direction',
			crossDomain: true,
			data: {
				'train': this.trainId,
				'route-id': this.routeDetails["route-id"]
			},
			dataType: 'text',
			success: (responseText) => {
				const responseJson = JSON.parse(responseText);
				this.drivingIsForwards = responseJson.direction === "forwards";
			},
			error: (jqXHR) => {
				if ('msg' in jqXHR.responseText) {
					const responseJson = JSON.parse(responseText);
					console.warn("/driver/direction error with returned message:", responseJson.msg);
				} else {
					console.warn("/driver/direction error with no message, status:", jqXHR.status);
				}
				setResponseDanger('#serverResponse', 
					'Could not find your train 😢', 
					'Dein Zug konnte nicht gefunden werden 😢', 
					''
				);
			}
		});
	}


	// Request to set the train's speed
	setTrainSpeedPromise(speed) {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/driver/set-dcc-train-speed',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'speed': this.drivingIsForwards ? speed : "-" + speed,
				'track-output': this.trackOutput
			},
			dataType: 'text',
			error: (jqXHR) => {
				if ('msg' in jqXHR.responseText) {
					const responseJson = JSON.parse(responseText);
					console.warn("/driver/set-dcc-train-speed error with returned message:", responseJson.msg);
				} else {
					console.warn("/driver/set-dcc-train-speed error with no message, status:", jqXHR.status);
				}
				setResponseDanger('#serverResponse',
					'There was a problem setting the speed of your train 😢',
					'Es ist ein Problem beim Einstellen der Geschwindigkeit aufgetreten 😢',
					''
				);
			}
		});
	}


	// Request for the train's current segment and then determine
	// whether to show the destination reached button
	enableDestinationReachedPromise() {
		const destinationReachedTimeout = 100;
		this.destinationReachedInterval = setInterval(() => {
			return $.ajax({
				type: 'POST',
				url: serverAddress + '/monitor/train-state',
				crossDomain: true,
				data: { 'train': this.trainId },
				dataType: 'text',
				success: (responseText) => {
					const responseJson = JSON.parse(responseText);
					if (!(responseJson.on_track) || !Array.isArray(responseJson.occupied_segments)) {
						return;
					}

					const segmentIDs = responseJson.occupied_segments;
					const segments = segmentIDs.map(s => s.replace(/(a|b)$/, ''));

					///NOTE: Adjusted this to enable DestinationReached, when the train
					//       occupies the expected destination main segment (this.routeDetails['segment'])
					//       AND the train occupies 3 segments or less.
					//       Previously, DestinationReached would only be enabled if the train
					//       *exclusively* occupied the destination main segment. This was problematic
					//       for routes with a short main segment on the block at end of the route.
					//       Drivers were getting a warning for stopping, even though they were already
					//       close to the destination signal.
					///TODO: Test if this can be simplified by `&& this.routeDetails['segment'] in segments`
					if (segments.length <= 3) {
						for (let index in segments) {
							if (segments[index] === this.routeDetails['segment']) {
								this.clearDestinationReachedInterval();
								this.isDestinationReached = true;
								$('#endGameButton').show();
								$(window).unbind('beforeunload', pageRefreshWarning);
								return;
							}
						}
					}
					/* OLD VERSION - SEE NOTE ABOVE
					for (let index in segments) {
						if (segments[index] != this.routeDetails['segment']) {
							return;
						}
					}
					if (segments[0] == this.routeDetails['segment']) {
						this.clearDestinationReachedInterval();
						this.isDestinationReached = true;
						$('#endGameButton').show();
						$(window).unbind('beforeunload', pageRefreshWarning);
					}*/
				}
			});
		}, destinationReachedTimeout);
	}

	disableDestinationReached() {
		this.isDestinationReached = false;
	}

	// Request to release the train
	releaseTrainPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/driver/release-train',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId
			},
			dataType: 'text',
			success: (_responseText) => {
				console.log("Train was released: ", this.trainId);
				this.resetTrainSession();
				this.trainId = null;
			},
			error: (jqXHR) => {
				if ('msg' in jqXHR.responseText) {
					const responseJson = JSON.parse(responseText);
					console.warn("/driver/release-train error with returned message:", responseJson.msg);
				} else {
					console.warn("/driver/release-train error with no message, status:", jqXHR.status);
				}
				setResponseDanger('#serverResponse',
					'There was a problem ending your turn 🤔',
					'Es ist ein Problem beim Beenden deiner Runde aufgetreten 🤔',
					''
				);
			}
		});
	}

	// Request for a specific route ID
	requestRouteIdPromise(pRouteDetails) {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/driver/request-route-by-id',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'route-id': pRouteDetails['route-id']
			},
			dataType: 'text',
			success: (_responseText) => {
				this.routeDetails = pRouteDetails;
				setResponseSuccess('#serverResponse',
					'Start driving your train to your chosen destination 🥳',
					'Fahr deinen Zug zum ausgewählten Ziel 🥳',
					''
				);
			},
			error: (jqXHR) => {
				if ('msg' in jqXHR.responseText) {
					const responseJson = JSON.parse(responseText);
					console.warn("/driver/request-route-by-id error with returned message:", responseJson.msg);
				} else {
					console.warn("/driver/request-route-by-id error with no message, status:", jqXHR.status);
				}
				this.routeDetails = null;
				setResponseDanger('#serverResponse',
					'This route is currently not available',
					'Diese Route ist zurzeit leider nicht verfügbar',
					'Sorry'
				);
			}
		});
	}

	// Request to manually drive the granted route
	driveRoutePromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/driver/drive-route',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'route-id': this.routeDetails["route-id"],
				'mode': 'manual'
			},
			dataType: 'text',
			success: (_responseText) => {
				if (!this.hasValidTrainSession()) {
					// Driver has already released the train
					console.log("driveRoutePromise called, success, but no valid train session.");
				} else if (!this.hasRouteGranted()) {
					// if (!this.hasRouteGranted()) is true when driveRoute returns from server, 
					// that means the *client* (aka the state in this driver object) has already
					// released the route explicitly (which is done if the client clicks on "stop"
					// whilst on the "destination segment"). Meaning, the driver *has* stopped
					// the train, and driveRoute has now returned from the server -> this means
					// that the driver has most likely stopped the train before the server had to
					// stop it (server stops train if the destination signal is passed).
					setModalSuccess(modalMessages.drivingSuccess, 'Juhuu!');
				} else {
					// driveRoute returns, and driver has valid train session, and the *client* 
					// (aka the state in this driver object) "thinks" that the route has NOT
					// been released yet. Means, the driver has not pressed "stop" whilst on the
					// "destination segment", otherwise the client would have released the route
					// already. So, the server must have stopped the train when it passed the 
					// destination signal.
					// Note that the server then automatically has already released the route,
					// so calling that again is not necessary.
					this.routeDetails = null;

					const destinationClass = $('#destination').attr('class');
					let drivingInfringement = JSON.parse(JSON.stringify(modalMessages.drivingInfringement));
					drivingInfringement['body']['de'] = drivingInfringement['body']['de'].replace('${destination}', destinationClass);
					drivingInfringement['body']['en'] = drivingInfringement['body']['en'].replace('${destination}', destinationClass);
					setModalDanger(drivingInfringement, 'Zielsignal verpasst!');
				}
			},
			error: (jqXHR) => {
				if ('msg' in jqXHR.responseText) {
					const responseJson = JSON.parse(responseText);
					console.warn("/driver/drive-route error with returned message:", responseJson.msg);
				} else {
					console.warn("/driver/drive-route error with no message, status:", jqXHR.status);
				}
				setResponseDanger('#serverResponse',
					'Route to your chosen destination is currently unavailable 😢',
					'Die Route zu deinem ausgewählten Ziel ist aktuell nicht verfügbar 😢',
					'Sorry'
				);
			}
		});
	}


	// Request to release the granted route
	releaseRoutePromise() {
		if (!this.hasRouteGranted()) {
			return;
		}

		const routeId = this.routeDetails["route-id"];
		this.routeDetails = null;

		return $.ajax({
			type: 'POST',
			url: serverAddress + '/controller/release-route',
			crossDomain: true,
			data: { 'route-id': routeId },
			dataType: 'text',
			error: (jqXHR) => {
				if ('msg' in jqXHR.responseText) {
					const responseJson = JSON.parse(responseText);
					console.warn("/controller/release-route error with returned message:", responseJson.msg);
				} else {
					console.warn("/controller/release-route error with no message, status:", jqXHR.status);
				}
			}
		});
	}


	// Manage the business logic of manually driving a granted route
	async driveToPromise(route) {
		if (!this.hasValidTrainSession()) {
			setResponseDanger('#serverResponse', 
				'Could not find your train 😢', 
				'Dein Zug konnte nicht gefunden werden 😢', 
				''
			);
			return;
		}

		const lock = await Mutex.lock();

		if (this.hasRouteGranted()) {
			// If train already has a route, return.
			Mutex.unlock(lock);
			return;
		}

		const destinationSignal = route['destSignalID'];
		const pRouteDetails = route[destinationSignal];
		console.log("driveToPromise: driving a route to signal: ", destinationSignal);

		// Note: this.routeDetails is set to pRouteDetails in this.requestRouteIdPromise success handler!
		this.requestRouteIdPromise(pRouteDetails)                      // 1. Ensure that the chosen destination is still available
			.then(() => this.updateDrivingDirectionPromise())          // 2. Obtain the physical driving direction
			.then(() => $('#destinationsFormContainer').hide())        // 3. Prevent the driver from choosing another destination
			.then(() => disableAllDestinationButtons())
			.then(() => Mutex.unlock(lock))
			.then(() => this.setTrainSpeedPromise(1))                  // 4. Update the train lights to indicate the physical driving direction
			.then(() => wait(1)) // 1ms
			.then(() => this.setTrainSpeedPromise(0))
			.then(() => setChosenDestination(destinationSignal))       // 5. Show the chosen destination and possible train speeds to the driver
			.then(() => enableSpeedButtons(destinationSignal))
			.then(() => this.enableDestinationReachedPromise())        // 6. Start monitoring whether the train has reached the destination

			.then(() => this.driveRoutePromise())                      // 7. Start the manual driving mode for the granted route

			.then(() => disableSpeedButtons())                         // 8. Prevent the driver from driving past the destination
			.then(() => clearChosenDestination())

			.then(() => {
				if (!this.hasValidTrainSession()) {                      // 9. Check whether the driver has quit their session
					this.clearDestinationReachedInterval();
					throw new Error("Driver ended their session");
				}
			})
			.then(() => this.disableDestinationReached())              // 10. Remove the destination reached behaviour
			.then(() => this.updateCurrentBlockPromise())              // 11. Show the next possible destinations
			.then(() => updatePossibleDestinations(driver.currentBlock))
			.catch(() => Mutex.unlock(lock));                          // 12. Execution skips to here if the safety layer was triggered (step 9)
	}
}



/*************************************************************
 * UI update for client initialisation and game start and end
 * (train grab and release)
 */

// Update the user interface for driving when the user decides to grab a train
function startGameLogic() {
	// FIXME: On iOS, speech synthesis only works if it is first triggered by the user.
	speak("");

	if (driver.hasValidTrainSession()) {
		setResponseDanger('#serverResponse', 
			'You are already driving a train!', 
			'Du fährst aktuell schon einen Zug!', 
			''
		);
		return;
	}

	setResponseSuccess('#serverResponse', '⏳ Waiting ...', '⏳ Warten ...', '');

	driver.grabTrainPromise()
		.then(() => $('#trainSelection').hide())
		.then(() => $('#chosenTrain').show())
		.then(() => $('#endGameButton').show())
		.then(() => driver.updateCurrentBlockPromise())
		.then(() => updatePossibleDestinations(driver.currentBlock))
		.always(() => driver.clearTrainAvailabilityInterval());
}

// Update the user interface for driving when the user decides to release their train
function endGameLogic() {
	$('#endGameButton').hide();
	$('#destinationsFormContainer').hide();
	driver.clearUpdatePossibleDestinationsInterval();
	driver.clearDestinationReachedInterval();
	driver.resetTrainSession();
	disableAllDestinationButtons();
	driver.disableDestinationReached();
	disableSpeedButtons();
	clearChosenDestination();

	$('#trainSelection').show();
	$('#chosenTrain').hide();
	driver.updateTrainAvailability();
}

// Asynchronous wait for a duration in milliseconds.
function wait(duration) {
	return new Promise(resolve => setTimeout(resolve, duration));
}

// Initialisation of the user interface and game logic
function initialise() {
	driver = new Driver(
		'master',                                 // trackOutput
		'libtrain_engine_default (unremovable)',  // trainEngine
		null                                      // trainId
	);

	// This will try repeatedly to determine the name of the SWTbahn Platform which is being used,
	// once that is found it will load the destinations and the flag mapping.
	tryLoadDestinationsAndFlagMap();

	// Hide the chosen train.
	$('#chosenTrain').hide();

	// Hide the alert box for displaying server messages.
	$('#serverResponse').parent().hide();

	// Update all train selections.
	driver.updateTrainAvailability();


	//-----------------------------------------------------
	// Attach button behaviours
	//-----------------------------------------------------

	// Set the initial language.
	language = 'de';
	$('span:lang(de):not(span span)').show();
	$('span:lang(en):not(span span)').hide();

	// Handle language selection.
	$('#changeLang').click(function () {
		language = (language == 'en') ? 'de' : 'en';
		$('span:lang(de):not(span span)').toggle();
		$('span:lang(en):not(span span)').toggle();
	});

	// Set the text verbosity.
	isEasyMode = false;
	$('.normal').show();
	$('.easy').hide();

	// Handle the verbosity selection.
	$('#changeTips').click(function (event) {
		isEasyMode = !isEasyMode
		$('.normal').toggle();
		$('.easy').toggle();
	});

	// Hide the train driving buttons (destination selections).
	$('#endGameButton').hide();
	$('#destinationsFormContainer').hide();
	clearChosenDestination();
	driver.disableDestinationReached();

	// Handle train selection.
	$('.selectTrainButton').click(function (event) {
		let trainId = event.currentTarget.id;
		driver.trainId = trainId;
		setChosenTrain(trainId);
		startGameLogic();
	});


	disableAllDestinationButtons();


	// Initialise the click handler of each destination button.
	for (let i = 0; i < numberOfDestinationsMax; i++) {
		const destinationButton = $(`#${destinationNamePrefix}${i}`);
		destinationButton.click(function () {
			// Should check if button is enabled here
			if (destinationButton.hasClass("flagThemeDisabled")) {
				console.log("Route Driving was not attempted because it is disabled");
				setResponseDanger('#serverResponse', 
					'This route is currently not available', 
					'Diese Route ist zurzeit leider nicht verfügbar', 
					''
				);
			} else {
				const route = JSON.parse(destinationButton.val());
				driver.driveToPromise(route);
			}
		});
	}

	disableSpeedButtons();

	// Initialise the click handler of each speed button. 
	// `speed` here matching the button IDs, as you can see in the definition of `speedButtons`
	// and the HTML file.
	speedButtons.forEach(speed => {
		const speedButton = $(`#${speed}`);
		speedButton.click(function () {
			let speedButtonVal = speedButton.val();
			let speedAdjusted = Math.round(speedButtonVal * trainSpeedFactors.get(driver.trainId));
			driver.setTrainSpeedPromise(speedAdjusted);
			if (!driver.isDestinationReached) {
				$('#endGameButton').hide();

				if (speedButton.val() == '0') {
					// Driver has stopped the train, but has not reached the destination segment yet.
					// -> tell the driver to continue driving.
					setModal(modalMessages.drivingContinue);
				}

				// The page cannot be refreshed without ill consequences.
				// The train might not stop sensibly on the main segment of the destination
				$(window).bind("beforeunload", pageRefreshWarning);
			} else if (speedButton.val() == '0') {
				// Destination is reached and driver stops the train -> release route.
				driver.releaseRoutePromise();
			}
		});
	});

	$('#endGameButton').click(function () {
		if (!driver.hasValidTrainSession()) {
			endGameLogic();
			return;
		}

		setResponseSuccess('#serverResponse', 'Waiting... ⏳', 'Warten...⏳', '');

		driver.setTrainSpeedPromise(0)
			.then(() => wait(500)) // 500 ms
			.always(() => {
				driver.releaseTrainPromise();
				driver.releaseRoutePromise();
				endGameLogic();
			});

		setResponseSuccess('#serverResponse', 
			'Thank you for playing 😀', 
			'Danke fürs Spielen 😀', 
			'Danke fürs Spielen'
		);
	});
}


$(document).ready(() => {
	initialise();
});

/*************************************
 * Handlers for page refresh or close
 */

// Before page unload (refresh or close) behaviour
function pageRefreshWarning(event) {
	event.preventDefault();
	// Most web browsers will display a generic message instead!!
	let warnmessage = "";
	if (language == 'de') {
		warnmessage = "Bist du dir sicher, dass du die Seite neu laden oder verlassen willst? " +
			"Das Verlassen oder neu Laden wird den Zug unverfügbar machen! " + 
			"Bitte nur dann durchführen, wenn du darum gebeten wirst.";
	} else {
		warnmessage = "Are you sure you want to refresh or leave this page? " +
			"Leaving this page without ending your game will prevent others from grabbing your train. " +
			"Please only do so if requested to.";
	}
	return event.returnValue = warnmessage;
}

// During page unload (refresh or close) behaviour
// Only synchronouse statements will be executed by this handler!
// Promises will not be executed!
$(window).on("unload", (event) => {
	console.log("Unloading");
	if (driver.hasRouteGranted()) {
		const formData = new FormData();
		formData.append('route-id', driver.routeDetails['route-id']);
		navigator.sendBeacon(serverAddress + '/controller/release-route', formData);
	}
	if (driver.hasValidTrainSession()) {
		const formData = new FormData();
		formData.append('session-id', driver.sessionId);
		formData.append('grab-id', driver.grabId);
		navigator.sendBeacon(serverAddress + '/driver/release-train', formData);
	}
});
