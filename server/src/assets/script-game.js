var driver = null;       // Train driver logic.
var stopwatch = null;    // Manages and displays the elapsed time.

var gameSourceSignal = null;      // Initial source signal for the game.
var gameDestinationSignal = null; // Final destination signal for the game.

// Portion of the route preview sprite to show is a percentage of the total sprite height.
var routePreviewHeight = null;    

const allDestinationChoices = [
	'destination1',
	'destination2',
	'destination3'
];

var allPossibleDestinations = null;

const allPossibleDestinationsSwtbahnUltraloop = [
	{ source: 'signal5', destinations: { 'destination1': 'signal4' } },
	{ source: 'signal6', destinations: { 'destination2': 'signal7' } }
];

const allPossibleDestinationsSwtbahnStandard = [
	{ source: 'signal3', destinations: { 'destination1': 'signal6', 'destination2': 'noSignal2', 'destination3': 'noSignal3' } },
	{ source: 'signal6', destinations: { 'destination2': 'signal14' } },
	{ source: 'signal14', destinations: { 'destination3': 'signal19' } },
	{ source: 'signal12', destinations: { 'destination2': 'signal6' } },
];

const allPossibleDestinationsSwtbahnFull = [
	// FIXME: To be filled in.
];

function getRoutes(sourceSignal) {
	if (allPossibleDestinations == null) {
		return { index: null, destinations: null };
	}
	
	const index = allPossibleDestinations.findIndex(element => (element.source == sourceSignal));
	if (index == -1) {
		return { index: null, destinations: null };
	}
		
	const destinations = allPossibleDestinations.at(index).destinations;
	return { index: index, destinations: destinations };
}

const disabledButtonStyle = 'btn-outline-secondary';
const destinationEnabledButtonStyle = {
	'destination1': 'btn-dark',
	'destination2': 'btn-primary',
	'destination3': 'btn-info'
};
const destinationHighlightedButtonStyle = {
	'destination1': 'btn-dark',
	'destination2': 'btn-primary',
	'destination3': 'btn-info'
};

function disableAllDestinationButtons() {
	allDestinationChoices.forEach(choice => {
		$(`#${choice}`).val("");
		$(`#${choice}`).prop('disabled', true);
		$(`#${choice}`).removeClass(destinationEnabledButtonStyle[choice]);
		$(`#${choice}`).removeClass(destinationHighlightedButtonStyle[choice]);
		$(`#${choice}`).addClass(disabledButtonStyle);
	});
}

function highlightDestinationButton(choice) {
	$(`#${choice}`).addClass(destinationHighlightedButtonStyle[choice]);
}

function enableDestinationButton(choice, signal) {
	$(`#${choice}`).val(signal);
	$(`#${choice}`).prop('disabled', false);
	$(`#${choice}`).addClass(destinationEnabledButtonStyle[choice]);
	$(`#${choice}`).removeClass(disabledButtonStyle);
}

function updatePossibleRoutes(sourceSignal) {
	disableAllDestinationButtons();
	
	const routes = getRoutes(sourceSignal);
	if (routes.index == null) {
		$('#routePreview').css('background-position', 'top -100% right');
		return;
	}
	
	Object.keys(routes.destinations).forEach(choice => {
		enableDestinationButton(choice, routes.destinations[choice]);
	});
	
	$('#routePreview').css('background-position', `top ${routePreviewHeight * routes.index}% right`);
}

function finalDestinationCheck(sourceSignal) {
	if (sourceSignal == gameDestinationSignal) {
		stopwatch.stop();
		// FIXME: Replace with better game sounds and visual response.
		speak('JA, JA, JA!');
	}
}

var responseTimer = null;
const responseTimeout = 7000;

function setResponse(responseId, message, callback) {
	clearTimeout(responseTimer);
	
	$(responseId).text(message);
	callback();
	
	$(responseId).parent().fadeIn("fast");
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

function setResponseDanger(responseId, message) {
	setResponse(responseId, message, function() {
		$(responseId).parent().addClass('alert-danger');
		$(responseId).parent().addClass('alert-danger-blink');
		$(responseId).parent().removeClass('alert-success');
	});
	
	// FIXME: Replace with better game sounds.
	speak('NEIN, NEIN, NEIN!');
}

function setResponseSuccess(responseId, message) {
	setResponse(responseId, message, function() {
		$(responseId).parent().removeClass('alert-danger');
		$(responseId).parent().removeClass('alert-danger-blink');
		$(responseId).parent().addClass('alert-success');
	});
}

class Driver {
	sessionId = null;
	grabId = null;

	trackOutput = null;
	trainEngine = null;
	trainId = null;
	userId = null;

	sourceSignal = null;
	destinationSignal = null;
	routeId = null;
	
	routeRequestTimeout = null;
	retryRouteTimeout = null;

	constructor(trackOutput, trainEngine, trainId, userId) {
		this.sessionId = 0;
		this.grabId = -1;

		this.trackOutput = trackOutput;
		this.trainEngine = trainEngine;
		this.trainId = trainId;
		this.userId = userId;

		this.sourceSignal = null;
		this.destinationSignal = null;
		this.routeId = null;
		
		this.routeRequestTimeout = null;
		this.retryRouteTimeout = 1;   // seconds
	}
	
	get hasValidTrainSession() {
		return (this.sessionId != 0 && this.grabId != -1)
	}
	
	get hasRouteGranted() {
		return (this.routeId != null);
	}
	
	cancelRouteRequestTimeout() {
		clearTimeout(this.routeRequestTimeout);
		this.routeRequestTimeout = null;
	}
	
	grabTrainPromise() {
		return $.ajax({
			type: 'POST',
			url: '/driver/grab-train',
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
			
				setResponseSuccess('#serverResponse', '😁 Your train is ready');
				$('#startGameButton').hide();
				$('#endGameButton').show();
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 There was a problem starting your train');
			}
		});
	}

	stopTrainPromise() {
		return $.ajax({
			type: 'POST',
			url: '/driver/set-dcc-train-speed',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'speed': 0,
				'track-output': this.trackOutput
			},
			dataType: 'text',
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 There was a problem stopping your train');
			}
		});
	}

	releaseTrainPromise() {
		return $.ajax({
			type: 'POST',
			url: '/driver/release-train',
			crossDomain: true,
			data: {
				'session-id': this.sessionId, 
				'grab-id': this.grabId
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.sessionId = 0;
				this.grabId = -1;
			
				setResponseSuccess('#serverResponse', '😀 Thank you for playing');
				$('#startGameButton').show();
				$('#endGameButton').hide();
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '🤔 There was a problem ending your turn');
			}
		});
	}
	
	requestRoutePromise() {
		return $.ajax({
			type: 'POST',
			url: '/driver/request-route',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'source': this.sourceSignal,
				'destination': this.destinationSignal
			},
			dataType: 'text',
			// FIXME: On iOS, speech synthesis only works if it is first triggered by the user.
			success: (responseData, textStatus, jqXHR) => {
				this.routeId = responseData;
			},
			error: (responseData, textStatus, errorThrown) => {
				this.routeId = null;
				setResponseDanger('#serverResponse', responseData.responseText);
			}
		});
	}

	driveRoutePromise() {
		return $.ajax({
			type: 'POST',
			url: '/driver/drive-route',
			crossDomain: true,
			data: {
				'session-id': this.sessionId, 
				'grab-id': this.grabId, 
				'route-id': this.routeId
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.routeId = null;
				this.sourceSignal = this.destinationSignal;
				setResponseSuccess('#serverResponse', '🥳 Train was driven to your chosen destination');
			},
			error: (responseData, textStatus, errorThrown) => {
				this.routeId = null;
				setResponseDanger('#serverResponse', '😢 Train could not be driven to your chosen destination');
			}
		});
	}

	releaseRoutePromise() {
		if (this.routeId == null) {
			return;
		}

		return $.ajax({
			type: 'POST',
			url: '/controller/release-route',
			crossDomain: true,
			data: { 'route-id': this.routeId },
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.routeId = null;
			}
		});
	}

	async driveToPromise(destination) {
		if (!this.hasValidTrainSession) {
			setResponseDanger('#serverResponse', 'Your train could not be found 😢');
			return;
		}
	
		const lock = await Mutex.lock();
		
		if (this.hasRouteGranted) {
			Mutex.unlock(lock);
			return;
		}
		
		this.cancelRouteRequestTimeout();
		
		this.destinationSignal = $(`#${destination}`).val();
		await this.requestRoutePromise().catch(() => {});
		if (!this.hasRouteGranted)  {
			// Keep retrying until the route is granted, or until the player selects another destination.
			this.routeRequestTimeout = setTimeout(() => this.driveToPromise(destination), this.retryRouteTimeout*1000);
			setResponseSuccess('#serverResponse', '⏳ Waiting for your chosen destination to become available ...');
			Mutex.unlock(lock);
			return;
		}
		
		disableAllDestinationButtons();
		highlightDestinationButton(destination);
		Mutex.unlock(lock);

		setResponseSuccess('#serverResponse', '⏳ Driving your train to your chosen destination ...');
	
		this.driveRoutePromise()
			.catch(() => this.releaseRoutePromise())
			.always(() => {
				updatePossibleRoutes(this.sourceSignal);
				finalDestinationCheck(this.sourceSignal);
			});
	}
}

function initialise() {
	driver = new Driver(
		'master',                                 // trackOutput
		'libtrain_engine_default (unremovable)',  // trainEngine
		'cargo_db',                               // trainId
		'Bob Jones'                               // userId
	);
	
//	gameSourceSignal = 'signal5';
	gameSourceSignal = 'signal3';
	gameDestinationSignal = 'signal19';
	
	// FIXME: Portion of the route preview sprite to show is a percentage of the total sprite height
	routePreviewHeight = 24;
	
	// Display the train name and user name.
	$('#userDetails').html(`${driver.userId} <br /> is driving ${driver.trainId}`);
	
	// FIXME: Quick way to test other players.
	$('#userDetails').click(function () {
		// Set the source signal for the train's starting position.
		driver.userId = 'Anna Jones';
		driver.trainId = 'cargo_green';
		gameSourceSignal = 'signal12';
		gameDestinationSignal = 'signal19';
		
		$('#userDetails').html(`${driver.userId} <br /> is driving ${driver.trainId}`);
	});
	
	// Only show the button to start the game.
	$('#startGameButton').show();
	$('#endGameButton').hide();
	
	// Hide the alert box for displaying server messages.
	$('#serverResponse').parent().hide();
	
	// Set the possible destinations for the SWTbahn platform.
	allPossibleDestinations = allPossibleDestinationsSwtbahnStandard;
//	allPossibleDestinations = allPossibleDestinationsSwtbahnUltraloop;
	disableAllDestinationButtons();
	
	// Initialise the click handler of each destination button.
	allDestinationChoices.forEach(choice => {
		$(`#${choice}`).click(function () {
			driver.driveToPromise(choice);
		});
	});
	
	// Initialise stop watch for the player's turn.
	stopwatch = new Stopwatch('#elapsedTime');
	stopwatch.clear();
}

// Wait for a duration in milliseconds.
function wait(duration) { 
	return new Promise(resolve => setTimeout(resolve, duration));
}

$(document).ready(
	function () {
		//-----------------------------------------------------
		// Initialisation
		//-----------------------------------------------------
		initialise();


		//-----------------------------------------------------
		// Button behaviours
		//-----------------------------------------------------
		
		$('#startGameButton').click(function () {
			// FIXME: On iOS, speech synthesis only works if it is first triggered by the user.
			speak("");

			stopwatch.clear();
			
			if (driver.hasValidTrainSession) {
				setResponseDanger('#serverResponse', 'You are already driving a train!')
				return;
			}
			
			setResponseSuccess('#serverResponse', '⏳ Waiting ...');
			
			// Set the source signal for the train's starting position.
			driver.sourceSignal = gameSourceSignal;
			
			driver.grabTrainPromise()
				.then(() => updatePossibleRoutes(driver.sourceSignal))
				.then(() => stopwatch.start());			
		});

		$('#endGameButton').click(function () {
			stopwatch.stop();

			if (!driver.hasValidTrainSession) {
				setResponseSuccess('#serverResponse', '😀 Thank you for playing');
				$('#startGameButton').show();
				$('#endGameButton').hide();
				return;
			}
			
			setResponseSuccess('#serverResponse', '⏳ Waiting ...');
			
			driver.sourceSignal = null;
			
			driver.cancelRouteRequestTimeout();
			driver.stopTrainPromise()
				.then(() => wait(500))
				.then(() => driver.releaseRoutePromise())
				.always(() => {
					driver.releaseTrainPromise();
					updatePossibleRoutes(driver.sourceSignal); 
				});
		});

	}	
);



