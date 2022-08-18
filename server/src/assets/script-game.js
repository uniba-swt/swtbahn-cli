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

const disabledButtonStyle = 'btn-outline-secondary';
const destinationButtonStyle = {
	'destination1': 'btn-dark',
	'destination2': 'btn-primary',
	'destination3': 'btn-info'
};


var allPossibleDestinations = null;

const allPossibleDestinationsSwtbahnUltraloop = {
	'signal1': {
		'destination1': 'signal2a',
		'destination3': 'signal2b'
	},
	'signal2': {
		'destination1': 'signal3'
	},
	'signal3': {
		'destination1': 'signal1'
	}
};

function disableAllDestinations() {
	allDestinationChoices.forEach(choice => {
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

function setResponseDanger(responseId, message) {
	setResponse(responseId, message, function() {
		$(responseId).parent().addClass('alert-danger');
		$(responseId).parent().removeClass('alert-success');
	});
}

function setResponseSuccess(responseId, message) {
	setResponse(responseId, message, function() {
		$(responseId).parent().removeClass('alert-danger');
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
		dataType: 'text'
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

	const possibleDestinations = allPossibleDestinations[sourceSignal];
	Object.keys(possibleDestinations).forEach(choice => {
		$(`#${choice}`).val(possibleDestinations[choice]);
		$(`#${choice}`).prop('disabled', false);
		$(`#${choice}`).addClass(destinationButtonStyle[choice]);
		$(`#${choice}`).removeClass(disabledButtonStyle);
	});
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
			
			disableAllDestinations();
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
			
			updatePossibleRoutes();
			
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

	requestRoute()
		.then(driveRoute)
	.fail(releaseRoute);
}


function initialise() {
	trackOutput = 'master';
	sessionId = 0;
	grabId = -1;
	userId = 'Bob Jones';
	trainId = 'cargo_db';
	trainEngine = 'libtrain_engine_default (unremovable)';
	
	sourceSignal = 'signal1';

	// Display the train name and user name.
	$('#userDetails').html(`${userId} <br /> is driving ${trainId}`);
	
	// Only show the button to start the game.
	$('#startGameButton').show();
	$('#endGameButton').hide();

	// Hide the alert box for displaying server messages.
	$('#serverResponse').parent().hide();
	
	// Display the possible starting routes.
	allPossibleDestinations = allPossibleDestinationsSwtbahnUltraloop;
	updatePossibleRoutes();
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
			
			grabTrain();
		});

		$('#endGameButton').click(function () {
			if (sessionId == 0 || grabId == -1) {
				setResponseSuccess('#serverResponse', 'Thank you for playing 😀');
				$('#startGameButton').show();
				$('#endGameButton').hide();
				return;
			}
			
			setResponseSuccess('#serverResponse', 'Waiting ⏳');
			
			stopTrain().catch()
				.then(releaseTrain).catch()
				.then(releaseRoute);
		});

		$('#destination1').click(function () {
			driveToDestination('#destination1');
		});
		
		$('#destination2').click(function () {
			driveToDestination('#destination2');
		});
		
		$('#destination3').click(function () {
			driveToDestination('#destination3');
		});

	}	
);

