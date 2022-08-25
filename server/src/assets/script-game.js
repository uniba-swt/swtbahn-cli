var trackOutput = null;
var sessionId = null;
var grabId = null;
var userId = null;
var trainId = null;
var trainEngine = null;

var sourceSignal = null;
var destinationSignal = null;
var routeId = null;

const allDestinationChoices = [
	'destination1',
	'destination2',
	'destination3'
];

var allPossibleDestinations = null;

const allPossibleDestinationsSwtbahnStandard = [
	{ source: 'signal3', destinations: { 'destination1': 'signal6' } },
	{ source: 'signal6', destinations: { 'destination2': 'signal14' } },
	{ source: 'signal14', destinations: { 'destination3': 'signal19' } }
];

const allPossibleDestinationsSwtbahnFull = [
	// Empty.
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
const destinationButtonStyle = {
	'destination1': 'btn-dark',
	'destination2': 'btn-primary',
	'destination3': 'btn-info'
};

function disableAllDestinations() {
	allDestinationChoices.forEach(choice => {
		$(`#${choice}`).val("");
		$(`#${choice}`).prop('disabled', true);
		$(`#${choice}`).removeClass(destinationButtonStyle[choice]);
		$(`#${choice}`).addClass(disabledButtonStyle);
	});
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
    var msg = new SpeechSynthesisUtterance();
    var voices = speechSynthesis.getVoices();
    for (const voice of voices) {
        if (voice.lang == "de-DE") {
            msg.voice = voice;
            break;
        }
    }
    msg.text = text;
    msg.rate = 0.7;
    msg.lang = 'de';
    speechSynthesis.speak(msg);
}

function setResponseDanger(responseId, message) {
	setResponse(responseId, message, function() {
		$(responseId).parent().addClass('alert-danger');
		$(responseId).parent().addClass('alert-danger-blink');
		$(responseId).parent().removeClass('alert-success');
	});
	speak('NEIN, NEIN, NEIN!');
}

function setResponseSuccess(responseId, message) {
	setResponse(responseId, message, function() {
		$(responseId).parent().removeClass('alert-danger');
		$(responseId).parent().removeClass('alert-danger-blink');
		$(responseId).parent().addClass('alert-success');
	});
}

function grabTrain() {
	return $.ajax({
		type: 'POST',
		url: '/driver/grab-train',
		crossDomain: true,
		data: { 'train': trainId, 'engine': trainEngine },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			responseDataSplit = responseData.split(',');
			sessionId = responseDataSplit[0];
			grabId = responseDataSplit[1];
			
			setResponseSuccess('#serverResponse', 'Your train is ready 😁');
			$('#startGameButton').hide();
			$('#endGameButton').show();
		},
		error: function (responseData, textStatus, errorThrown) {
			setResponseDanger('#serverResponse', 'There was a problem starting your train 😢');
		}
	});
}

function stopTrain() {
	return $.ajax({
		type: 'POST',
		url: '/driver/set-dcc-train-speed',
		crossDomain: true,
		data: {
			'session-id': sessionId,
			'grab-id': grabId,
			'speed': 0,
			'track-output': trackOutput
		},
		dataType: 'text',
		error: function (responseData, textStatus, errorThrown) {
			setResponseDanger('#serverResponse', 'There was a problem stopping your train 😢');
		}
	});
}

function releaseTrain() {
	return $.ajax({
		type: 'POST',
		url: '/driver/release-train',
		crossDomain: true,
		data: { 'session-id': sessionId, 'grab-id': grabId },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			sessionId = 0;
			grabId = -1;
			
			setResponseSuccess('#serverResponse', 'Thank you for playing 😀');
			$('#startGameButton').show();
			$('#endGameButton').hide();
		},
		error: function (responseData, textStatus, errorThrown) {
			setResponseDanger('#serverResponse', 'There was a problem ending your turn 🤔');
		}
	});
}

function updatePossibleRoutes() {
	disableAllDestinations();
	
	const routes = getRoutes(sourceSignal);
	if (routes.index == null) {
		$('#routePreview').css('background-position', 'top -46% right');
		return;
	}
	
	Object.keys(routes.destinations).forEach(choice => {
		$(`#${choice}`).val(routes.destinations[choice]);
		$(`#${choice}`).prop('disabled', false);
		$(`#${choice}`).addClass(destinationButtonStyle[choice]);
		$(`#${choice}`).removeClass(disabledButtonStyle);
	});
	
	$('#routePreview').css('background-position', `top ${46 * routes.index}% right`);
}

function requestRoute() {
	return $.ajax({
		type: 'POST',
		url: '/driver/request-route',
		crossDomain: true,
		data: {
			'session-id': sessionId,
			'grab-id': grabId,
			'source': sourceSignal,
			'destination': destinationSignal
		},
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			routeId = responseData;
		},
		error: function (responseData, textStatus, errorThrown) {
			routeId = null;
			setResponseDanger('#serverResponse', responseData.responseText);
		}
	});
}

function driveRoute() {
	return $.ajax({
		type: 'POST',
		url: '/driver/drive-route',
		crossDomain: true,
		data: { 'session-id': sessionId, 'grab-id': grabId, 'route-id': routeId },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			routeId = null;
			sourceSignal = destinationSignal;
			setResponseSuccess('#serverResponse', 'Train was driven to your chosen destination 🥳');
		},
		error: function (responseData, textStatus, errorThrown) {
			setResponseDanger('#serverResponse', 'Train could not be driven to your chosen destination 😢');
		}
	});
}

function releaseRoute() {
	if (routeId == null) {
		return;
	}

	return $.ajax({
		type: 'POST',
		url: '/controller/release-route',
		crossDomain: true,
		data: { 'route-id': routeId },
		dataType: 'text'
	});
}
	
function driveToDestination(destination) {
	if (sessionId == 0 || grabId == -1) {
		setResponseDanger('#serverResponse', 'Your train could not be found 😢');
		return;
	}
	
	setResponseSuccess('#serverResponse', 'Driving your train to your chosen destination ⏳');

	destinationSignal = $(destination).val();
	disableAllDestinations();

	requestRoute().then(driveRoute)
		.fail(releaseRoute).always(updatePossibleRoutes);
}


function initialise() {
	trackOutput = 'master';
	sessionId = 0;
	grabId = -1;
	userId = 'Bob Jones';
	trainId = 'cargo_db';
	trainEngine = 'libtrain_engine_default (unremovable)';	

	// Display the train name and user name.
	$('#userDetails').html(`${userId} <br /> is driving ${trainId}`);
	
	// Only show the button to start the game.
	$('#startGameButton').show();
	$('#endGameButton').hide();

	// Hide the alert box for displaying server messages.
	$('#serverResponse').parent().hide();
	
	// Set the possible destinations for the SWTbahn platform.
	allPossibleDestinations = allPossibleDestinationsSwtbahnStandard;
	disableAllDestinations();
	
	// Initialise the click handler of each destination button.
	allDestinationChoices.forEach(choice => {
		$(`#${choice}`).click(function () {
			driveToDestination(`#${choice}`);
		});
	});
}

function resetSourceSignal() {
	// Set the source signal for the train's starting position.
	sourceSignal = 'signal3';
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
			if (sessionId != 0 || grabId != -1) {
				setResponseDanger('#serverResponse', 'You are already driving a train!')
				return;
			}
			
			setResponseSuccess('#serverResponse', 'Waiting ⏳');
			
			resetSourceSignal();
			
			grabTrain().then(updatePossibleRoutes);
		});

		$('#endGameButton').click(function () {
			if (sessionId == 0 || grabId == -1) {
				setResponseSuccess('#serverResponse', 'Thank you for playing 😀');
				$('#startGameButton').show();
				$('#endGameButton').hide();
				return;
			}
			
			setResponseSuccess('#serverResponse', 'Waiting ⏳');
			
			sourceSignal = null;
			
			stopTrain()
				.then(() => wait(500))
				.then(releaseTrain)
				.always(() => { 
					releaseRoute(); 
					updatePossibleRoutes(); 
				});
		});

	}	
);

