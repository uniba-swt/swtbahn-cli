var driver = null;           // Train driver logic.
var drivingTimer = null;     // Timer for driving a train.
var serverAddress = "";      // The base address of the server.
var language = "";           // User interface language.

/**************************************************
 * Destination related information and UI elements
 */

const numberOfDestinationsMax = 12;           // Maximum destinations to display
const destinationNamePrefix = "destination";  // HTML element ID prefix of the destination buttons

var allPossibleDestinations = null;           // Platform specific lookup table for destinations
var signalFlagMap = null;                     // Platform specific lookup table for signal flags

// Returns the destinations possible from a given block
function getDestinations(blockId) {
	if (allPossibleDestinations == null || !allPossibleDestinations.hasOwnProperty(blockId)) {
		return null;
	}
	return allPossibleDestinations[blockId];
}

// Destructures a route into its destination signal and route details
function unpackRoute(route) {
	for (let destinationSignal in route) {
		return [destinationSignal, route[destinationSignal]];
	}
	return null;
}

function disableAllDestinationButtons() {
	driver.clearUpdatePossibleDestinationsInterval();

	for (let i = 0; i < numberOfDestinationsMax; i++) {
		$(`#${destinationNamePrefix}${i}`).val("");
		$(`#${destinationNamePrefix}${i}`).attr("class", "flagThemeBlank");
	}
}

function setDestinationButton(choice, route) {
	const [destinationSignal, routeDetails] = unpackRoute(route);
	const destination = destinationSignal.replace(/(a|b)$/, '');

	// Route details are stored in the value parameter of the destination button
	$(`#${destinationNamePrefix}${choice}`).val(JSON.stringify(route));
	$(`#${destinationNamePrefix}${choice}`).attr("class", signalFlagMap[destination]);
}

function setDestinationButtonAvailable(choice, route) {
	setDestinationButton(choice, route);
	$(`#${destinationNamePrefix}${choice}`).removeClass("flagThemeDisabled");
}

function setDestinationButtonUnavailable(choice, route) {
	setDestinationButton(choice, route);
	$(`#${destinationNamePrefix}${choice}`).addClass("flagThemeDisabled");
}

function setChosenTrain(trainId) {
	const imageName = `train-${trainId.replace("_", "-")}.jpg`
	$("#chosenTrain").attr("src", imageName);
}

// Periodically update the availability of a blocks possible destinations.
// Update can be stopped by cancelling updatePossibleDestinationsInterval,
// e.g., when disabling the destination buttons or ending the game.
function updatePossibleDestinations(blockId) {
	// Show the form that contains all the destination buttons
	$('#destinationsForm').show();
	disableAllDestinationButtons();

	// Set up a timer interval to periodically update the availability
	const updatePossibleDestinationsTimeout = 1000;
	driver.updatePossibleDestinationsInterval = setInterval(() => {
		console.log("Checking available destinations ...");

		const routes = getDestinations(blockId);
		if (routes == null) {
			return;
		}

		Object.keys(routes).forEach((destinationSignal, choice) => {
			const route = { };
			route[destinationSignal] = routes[destinationSignal];

			const [_destinationSignal, routeDetails] = unpackRoute(route);
			let routeId = routeDetails["route-id"];
			updateDestinationAvailabilityPromise(
				routeId,
				// route is available
				() => setDestinationButtonAvailable(choice, route),
				// route is unavailable
				() => setDestinationButtonUnavailable(choice, route)
			);
		});
	}, updatePossibleDestinationsTimeout);
}

// Server request for a route's status and then determine whether the
// route is available
function updateDestinationAvailabilityPromise(routeId, available, unavailable) {
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/monitor/route',
		crossDomain: true,
		data: {
			'route-id': routeId,
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			const isNotConflicting = responseData.includes("granted conflicting route ids: none");
			const isRouteClear = responseData.includes("route clear: yes");
			const isNotGranted = responseData.includes("granted train: none");

			const isAvailable = isNotConflicting && isRouteClear && isNotGranted;
			if (isAvailable) {
				available();
			} else {
				unavailable();
			}
		},
		error: (responseData, textStatus, errorThrown) => {
			setResponseDanger('#serverResponse', 
				'😢 There was a problem checking the destinations', 
				'😢 Es ist ein Problem beim Überprüfen der Ziele aufgetreten',
				'Sorry'
			);
		}
	});

}


/**************************************************
 * Train speed UI elements
 */

const speedButtons = [
	"stop",
	"slow",
	"normal",
	"fast"
];

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

function clearChosenDestination() {
	$('#destination').attr("class", "flagThemeBlank");
}

function setChosenDestination(destination) {
	destination = destination.replace(/(a|b)$/, '');
	$('#destination').attr("class", signalFlagMap[destination]);
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
	var msg = new SpeechSynthesisUtterance(text);
	for (const voice of window.speechSynthesis.getVoices()) {
		if (voice.lang == "de-DE") {
			msg.voice = voice;
			break;
		}
	}
	window.speechSynthesis.speak(msg);
}

function setResponseDanger(responseId, messageEn, messageDe, messageSpeak) {
	setResponse(responseId, messageEn, messageDe, function() {
		$(responseId).parent().addClass('alert-danger');
		$(responseId).parent().addClass('alert-danger-blink');
		$(responseId).parent().removeClass('alert-success');
	});

	speak(messageSpeak);
}

function setResponseSuccess(responseId, messageEn, messageDe, messageSpeak) {
	setResponse(responseId, messageEn, messageDe, function() {
		$(responseId).parent().removeClass('alert-danger');
		$(responseId).parent().removeClass('alert-danger-blink');
		$(responseId).parent().addClass('alert-success');
	});

	speak(messageSpeak);
}

const modalMessages = {
	drivingInfringement: {
		title: {
			de: '👎 Fahrt nicht zulässig!',
			en: '👎 Driving Infringement!'
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
			de: 'Du hast dein Ziel noch nicht erreicht. Bitte fahr weiter 😀',
			en: 'You have not yet reached your destination. Please continue driving 😀'
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
			de: '🥳 Du hast deinen Zug zur deiner ausgewählten Station gefahren',
			en: '🥳 You drove your train to your chosen destination!'
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

	reset() {
		this.sessionId = 0;
		this.grabId = -1;
	}

	get hasValidTrainSession() {
		return (this.sessionId != 0 && this.grabId != -1)
	}

	get hasRouteGranted() {
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

	// Server request for the train's current block
	updateCurrentBlockPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/monitor/train-state',
			crossDomain: true,
			data: {
				'train': this.trainId
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				const regexMatch = /on block: (.*?) /g.exec(responseData);
				this.currentBlock =  regexMatch[1];
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', 
					'😢 Could not find your train', 
					'😢 Dein Zug konnte nicht gefunden werden',
					''
				);
			}
		});
	}

	// Server request for a train's status and then execute the callback handlers
	trainIsAvailablePromise(trainId, success, error) {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/monitor/train-state',
			crossDomain: true,
			data: { 'train': trainId },
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				if (responseData.includes("grabbed: no") && !responseData.includes("on segment: no")) {
					success();
				} else {
					error();
				}
			},
			error: (responseData, textStatus, errorThrown) => {
				// Do nothing
			}
		});
	}

	// Update the styling of the train selection buttons based on the train availabilities
	updateTrainAvailability() {
		$('.selectTrainButton').prop("disabled", true);
		
		const trainAvailabilityTimeout = 1000;
		this.trainAvailabilityInterval = setInterval(() => {
			console.log("Checking available trains ... ");

			// Enable a train if it is on the tracks and has not been grabbed
			$('.selectTrainButton').each((index, obj) => {
				let trainId = obj.id;
				this.trainIsAvailablePromise(
					trainId,
					() => { 
						$(obj).prop("disabled", false);
						$(obj).removeClass("btn-danger");
						$(obj).addClass("btn-primary");
						$($(obj).parent(".card-body").parent(".card")).removeClass("unavailableTrain");
						$(obj).children().each(function() {
							switch($(this).attr('lang')){
								case "de": $(this).html("<span class='easy'>Drücke hier um den Zug zu steuern</span><span class='normal'>Fahre den Zug</span>"); break;
								case "en": $(this).html("<span class='easy'>Press here to control this train</span><span class='normal'>Drive this train</span>"); break;
							}

						});
						if(isEasyMode){
							$('.normal').hide();
							$('.easy').show();
						}else{
							$('.normal').show();
							$('.easy').hide();
						}
					},
					() => {
						$(obj).prop("disabled", true);
						$(obj).removeClass("btn-primary");
						$(obj).addClass("btn-danger");
						$($(obj).parent(".card-body").parent(".card")).addClass("unavailableTrain");
						$(obj).children().each(function() {
							switch($(this).attr('lang')){
								case "de": $(this).text("Nicht verfügbar"); break;
								case "en": $(this).text("Unavailable"); break;
							}
						});
					}
				);
			})
		}, trainAvailabilityTimeout);
	}


	// Server request to grab a train
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
			success: (responseData, textStatus, jqXHR) => {
				const responseDataSplit = responseData.split(',');
				this.sessionId = responseDataSplit[0];
				this.grabId = responseDataSplit[1];

				setResponseSuccess('#serverResponse', '😁 Your train is ready', '😁 Dein Zug ist bereit', '');
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', 
					'😢 There was a problem starting your train', 
					'😢 Es ist ein Problem beim starten deines Zuges aufgetreten',
					''
				);
			}
		});
	}

	// Server request for the train's physical driving direction of its granted route
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
			success: (responseData, textStatus, jqXHR) => {
				this.drivingIsForwards = responseData.includes("forwards");
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 Could not find your train', '😢 Dein Zug konnte nicht gefunden werden', '');
			}
		});
	}

	// Server request to set the train's speed
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
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', 
					'😢 There was a problem setting the speed of your train', 
					'😢 Es ist ein Problem beim Einstellen der Geschwindigkeit deines Zuges aufgetreten',
					''
				);
			}
		});
	}

	// Server request for the train's current segment and then determine
	// whether to show the destination reached button
	enableDestinationReachedPromise() {
		const destinationReachedTimeout = 100;
		this.destinationReachedInterval = setInterval(() => {
			return $.ajax({
				type: 'POST',
				url: serverAddress + '/monitor/train-state',
				crossDomain: true,
				data: {
					'train': this.trainId
				},
				dataType: 'text',
				success: (responseData, textStatus, jqXHR) => {
					const matches = /on segment: (.*?) -/g.exec(responseData); // Get all Segment IDS as String
					const segmentIDs = matches[1];
					const segments = segmentIDs.split(", "); // Splits them into Array

					// Show the destination reached button when the train is only on the
					// main segment of the destination
					// Take into account that a main segment could be split into a/b segments
					for (let index in segments) {
						segments[index] = segments[index].replace(/(a|b)$/, '');
						if (segments[index] != this.routeDetails['segment']) {
							return;
						}
					}
					
					if(segments[0] == this.routeDetails['segment']) {
						this.clearDestinationReachedInterval();
						this.isDestinationReached = true;
						$('#endGameButton').show();

						// The page can be refreshed without ill consequences.
						// The train will stop sensibly on the main segment of the destination
						$(window).unbind('beforeunload', pageRefreshWarning);
					}
				}
			});
		}, destinationReachedTimeout);
	}
	
	disableDestinationReached() {
		this.isDestinationReached = false;
	}

	// Server request to release the train
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
			success: (responseData, textStatus, jqXHR) => {
				this.reset();
				this.trainId = null;
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', 
					'🤔 There was a problem ending your turn', 
					'🤔 Es ist ein Problem beim Beenden deiner Runde aufgetreten',
					''
				);
			}
		});
	}

	// Server request for a specific route ID
	requestRouteIdPromise(routeDetails) {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/driver/request-route-id',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'route-id': routeDetails['route-id']
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.routeDetails = routeDetails;
				setResponseSuccess('#serverResponse', 
					'🥳 Start driving your train to your chosen destination', 
					'🥳 Fahr deinen Zug zum ausgewählten Ziel',
					''
				);
			},
			error: (responseData, textStatus, errorThrown) => {
				this.routeDetails = null;
				setResponseDanger('#serverResponse', "This route is not available", "Diese Route ist zurzeit leider nicht verfügbar", 'Sorry');
			}
		});
	}

	// Server request to manually drive the granted route
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
			success: (responseData, textStatus, jqXHR) => {
				if (!this.hasValidTrainSession) {
					// Ignore, driver has ended their trip
				} else if (!this.hasRouteGranted) {
					setModalSuccess(modalMessages.drivingSuccess, 'Juhuu!!');
				} else {
					this.routeDetails = null;
					
					// Copy the driving infringement message and fill in the destination flag
					const destinationClass = $('#destination').attr('class');
					let drivingInfringement = JSON.parse(JSON.stringify(modalMessages.drivingInfringement));
					drivingInfringement['body']['de'] = drivingInfringement['body']['de'].replace('${destination}', destinationClass);
					drivingInfringement['body']['en'] = drivingInfringement['body']['en'].replace('${destination}', destinationClass);
					setModalDanger(drivingInfringement, 'STOP, STOP, STOP');
				}
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', 
					'😢 Route to your chosen destination is unavailable', 
					'😢 Die Route zu deinem ausgewählten Ziel ist aktuell nicht verfügbar',
					'Sorry'
				);
			}
		});
	}

	// Server request to release the granted route
	releaseRoutePromise() {
		if (!this.hasRouteGranted) {
			return;
		}
		
		const routeId = this.routeDetails["route-id"];
		this.routeDetails = null;

		return $.ajax({
			type: 'POST',
			url: serverAddress + '/controller/release-route',
			crossDomain: true,
			data: { 'route-id': routeId },
			dataType: 'text'
		});
	}

	// Manage the business logic of manually driving a granted route
	async driveToPromise(route) {
		if (!this.hasValidTrainSession) {
			setResponseDanger('#serverResponse', '😢 Could not find your train', '😢 Dein Zug konnte nicht gefunden werden', '');
			return;
		}
		
		const lock = await Mutex.lock();
		
		if (this.hasRouteGranted) {
			Mutex.unlock(lock);
			return;
		}

		const [destinationSignal, routeDetails] = unpackRoute(route);
        console.log(routeDetails);
        
		this.requestRouteIdPromise(routeDetails)                       // 1. Ensure that the chosen destination is still available
			.then(() => this.updateDrivingDirectionPromise())          // 2. Obtain the physical driving direction
			.then(() => $('#destinationsForm').hide())                 // 3. Prevent the driver from choosing another destination
			.then(() => disableAllDestinationButtons())
			.then(() => Mutex.unlock(lock))
			.then(() => this.setTrainSpeedPromise(1))                  // 4. Update the train lights to indicate the physical driving direction
			.then(() => wait(1))
			.then(() => this.setTrainSpeedPromise(0))
			.then(() => setChosenDestination(destinationSignal))       // 5. Show the chosen destination and possible train speeds to the driver
			.then(() => enableSpeedButtons(destinationSignal))
			.then(() => this.enableDestinationReachedPromise())        // 6. Start monitoring whether the train has reached the destination

			.then(() => this.driveRoutePromise())                      // 7. Start the manual driving mode for the granted route

			.then(() => disableSpeedButtons())                         // 8. Prevent the driver from driving past the destination
			.then(() => clearChosenDestination())

			.then(() => {
				if (!this.hasValidTrainSession) {                      // 9. Check whether the driver has quit their session
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

	if (driver.hasValidTrainSession) {
		setResponseDanger('#serverResponse', 'You are already driving a train!', 'Du fährst aktuell schon einen Zug!', 'STOP!')
		return;
	}

	setResponseSuccess('#serverResponse', '⏳ Waiting ...', '⏳ Warten ...');

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
	$('#destinationsForm').hide();
	driver.clearUpdatePossibleDestinationsInterval();
	driver.clearDestinationReachedInterval();
	driver.reset();
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
	$('span:lang(en)').hide();
	$('span:lang(de)').show();
	language = 'de';
	
	// Handle language selection.
	$('#changeLang').click(function () {
		$('span:lang(en)').toggle();
		$('span:lang(de)').toggle();
		language = (language == 'en') ? 'de' : 'en';
	});

	isEasyMode = false;
	$('#changeDescription').click(function (){
		if(isEasyMode){
			isEasyMode = false;
		}else{
			isEasyMode = true;
		}
	});

	// Hide the train driving buttons (destination selections).
	$('#endGameButton').hide();
	$('#destinationsForm').hide();
	clearChosenDestination();
	driver.disableDestinationReached();

	// Handle train selection.
	$('.selectTrainButton').click(function (event) {
		let trainId = event.currentTarget.id;
		driver.trainId = trainId;
		setChosenTrain(trainId);
		startGameLogic();
	});

	// Set the possible destinations for the SWTbahn platform.
	allPossibleDestinations = allPossibleDestinationsSwtbahnFull;
//	allPossibleDestinations = allPossibleDestinationsSwtbahnStandard;
//	allPossibleDestinations = allPossibleDestinationsSwtbahnUltraloop;
	disableAllDestinationButtons();

	// Set the signal to flag mapping.
	signalFlagMap = signalFlagMapSwtbahnFull;

	// Initialise the click handler of each destination button.
	for (let i = 0; i < numberOfDestinationsMax; i++) {
		const destinationButton = $(`#${destinationNamePrefix}${i}`);
		destinationButton.click(function () {
			// Should check if button is enabled here
			if (destinationButton.hasClass("flagThemeDisabled")) {
				console.log("Route Driving was not attempted because it is disabled");
				setResponseDanger('#serverResponse', "This route is not available", "Diese Route ist zurzeit leider nicht verfügbar", 'Sorry');
			} else {
				const route = JSON.parse(destinationButton.val());
				driver.driveToPromise(route);
			}
		});
	}

	disableSpeedButtons();

	// Initialise the click handler of each speed button.
	speedButtons.forEach(speed => {
		const speedButton = $(`#${speed}`);
		speedButton.click(function () {
			driver.setTrainSpeedPromise(speedButton.val());
			if (!driver.isDestinationReached) {
				$('#endGameButton').hide();

				if (speedButton.val() == '0') {
					setModal(modalMessages.drivingContinue);
				}
				
				// The page cannot be refreshed without ill consequences.
				// The train might not stop sensibly on the main segment of the destination
				$(window).bind("beforeunload", pageRefreshWarning);
			} else if (speedButton.val() == '0') {
				driver.releaseRoutePromise();
			}
		});
	});

	$('#endGameButton').click(function () {
		if (!driver.hasValidTrainSession) {
			endGameLogic();
			return;
		}

		setResponseSuccess('#serverResponse', '⏳ Waiting ...', '⏳ Warten ...');

		driver.setTrainSpeedPromise(0)
			.then(() => wait(500))
			.always(() => {
				driver.releaseTrainPromise();
				driver.releaseRoutePromise();
				endGameLogic();
			});

		setResponseSuccess('#serverResponse', '😀 Thank you for playing', '😀 Danke fürs Spielen', 'Danke fürs Spielen');
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
	console.log("Before unloading");

	// Most web browsers will display a generic message instead!!
	const message = "Are you sure you want to refresh or leave this page? " +
		"Leaving this page without ending your game will prevent others from grabbing your train 😕";
	return event.returnValue = message;
}

// During page unload (refresh or close) behaviour
// Only synchronouse statements will be executed by this handler!
// Promises will not be executed!
$(window).on("unload", (event) => {
	console.log("Unloading");
	if (driver.hasRouteGranted) {
		const formData = new FormData();
		formData.append('route-id', driver.routeDetails['route-id']);
		navigator.sendBeacon(serverAddress + '/controller/release-route', formData);
	}
	if (driver.hasValidTrainSession) {
		const formData = new FormData();
		formData.append('session-id', driver.sessionId);
		formData.append('grab-id', driver.grabId);
		navigator.sendBeacon(serverAddress + '/driver/release-train', formData);
	}
});
