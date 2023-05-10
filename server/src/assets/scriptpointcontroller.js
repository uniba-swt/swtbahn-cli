var trackOutput = 'master';
var sessionId = 0;

const pointAspects = ['normal', 'reverse'];

const ptAspectMapperArray = [
	["point1", ""],
	["point2", ""],
	["point3", ""],
	["point4", ""],
	["point5", ""],
	["point6", ""],
	["point7", ""],
	["point8", ""],
	["point9", ""],
	["point10", ""],
	["point11", ""],
	["point12", ""],
	["point13", ""],
	["point14", ""],
	["point15", ""],
	["point16", ""],
	["point17", ""],
	["point18a", ""],
	["point18b", ""],
	["point19", ""],
	["point20", ""],
	["point21", ""],
	["point22", ""],
	["point23", ""],
	["point24", ""],
	["point25", ""],
	["point26", ""],
	["point27", ""],
	["point28", ""],
	["point29", ""]
];
var ptAspectMap = new Map(ptAspectMapperArray);

var sigPossibleAspects = new Map();

var sigAspectMap = new Map([
	["signal1", ""],
	["signal2", ""],
	["signal3", ""],
	["signal4a", ""],
	["signal5", ""],
	["signal6", ""],
	["signal7a", ""],
	["signal8", ""],
	["signal9", ""],
	["signal10", ""],
	["signal11", ""],
	["signal12", ""],
	["signal13", ""],
	["signal14", ""],
	["signal15", ""],
	["signal16", ""],
	["signal17", ""],
	["signal18a", ""],
	["signal19", ""],
	["signal20", ""],
	["signal21", ""],
	["signal22a", ""],
	["signal23", ""],
	["signal24", ""],
	["signal25", ""],
	["signal26", ""],
	["signal27", ""],
	["signal28", ""],
	["signal29", ""],
	["signal30", ""],
	["signal31", ""],
	["signal32", ""],
	["signal33", ""],
	["signal34", ""],
	["signal35a", ""],
	["signal36", ""],
	["signal37", ""],
	["signal38", ""],
	["signal39", ""],
	["signal40", ""],
	["signal41", ""],
	["signal42", ""],
	["signal43", ""],
	["signal44", ""],
	["signal45", ""],
	["signal46a", ""],
	["signal47", ""],
	["signal48a", ""],
	["signal49", ""],
	["signal50a", ""],
	["signal51", ""],
	["signal52", ""],
	["signal53a", ""]
]);

function setPointAjax(pointId, pointAspect) {
	$.ajax({
		type: 'POST',
		url: '/controller/set-point',
		crossDomain: true,
		data: { 'point': pointId, 'state': pointAspect },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log("set point " + pointId + " to " + pointAspect);
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log("Error when setting point " + pointId + " to " + pointAspect);
		}
	});
}


function getPointsAspectsAndUpdateMap() {
	$.ajax({
		type: 'POST',
		url: '/monitor/points',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log('Got point aspects');
			let matchArr;
			const ptAspRegex = /(\w+) - state: (\w+)/g;
			var respSplit = responseData.split(/\r?\n/);
			for (const elem of respSplit) {
				matchArr = [...elem.matchAll(ptAspRegex)];
				if (matchArr !== null) {
					ptAspectMap[matchArr[0][1]] = matchArr[0][2];
					//console.log(matchArr[0][1] + " aspect: " + matchArr[0][2]);
				}
			}
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log('Could not get point aspects');
		}
	});
}

function setSignalAjax(signalId, signalAspect) {
	$.ajax({
		type: 'POST',
		url: '/controller/set-signal',
		crossDomain: true,
		data: { 'signal': signalId, 'state': signalAspect },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log('Signal ' + signalId + ' set to ' + signalAspect);
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log('Set signal failed: ' + responseData);
		}
	});
}

function getSignalsAspectsPromise(onSuccess, onErr) {
	return $.ajax({
		type: 'POST',
		url: '/monitor/signals',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			onSuccess(responseData);
		},
		error: function (responseData, textStatus, errorThrown) {
			onErr(responseData);
		}
	});
}

function getPossibleSignalAspectsPromise(signalId, onSuccess, onErr) {
	return $.ajax({
		type: 'POST',
		url: '/monitor/signal-aspects',
		crossDomain: true,
		data: {
			'signal': signalId,
		},
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			onSuccess(responseData);
		},
		error: function (responseData, textStatus, errorThrown) {
			onErr(responseData);
		}
	});
}

function updateSignalsAspectsPromise(onSuccess, onErr) {
	return getSignalsAspectsPromise(
		// Success
		(res) => {
			console.log('Got signal aspects');
			let matchArr;
			const sigAspRegex = /(\w+) - state: (\w+)/g;
			var respSplit = res.split(/\r?\n/);
			for (const elem of respSplit) {
				matchArr = [...elem.matchAll(sigAspRegex)];
				if (matchArr !== null) {
					sigAspectMap[matchArr[0][1]] = matchArr[0][2];
					//console.log(matchArr[0][1] + " aspect: " + matchArr[0][2]);
				}
			}
			onSuccess();
		},
		// Err
		(res) => {
			console.log('Could not get signal aspects');
			onErr();
		}
	);
}


async function switchPoint(pointID) {
	getPointsAspectsAndUpdateMap();
	await new Promise(r => setTimeout(r, 500));
	let aspect = "reverse";
	if (ptAspectMap.get(pointID) === "reverse") {
		aspect = "normal";
	}
	setPointAjax(pointID, aspect);
	ptAspectMap[pointID] = aspect;
	updatePointVisuals();
}

function updateAndSetSignalToNextAspectPromise(signalID) {
	return updateSignalsAspectsPromise(()=>{
		// determine position of current aspect of signal in the list of possible aspects for this signal.
		var currAspect = sigAspectMap[String(signalID)];
		var posOfAspect = sigPossibleAspects.get(signalID).findIndex((elem) => elem === currAspect);
		//console.log("Current aspect of " + signalID + ": " + currAspect);
		if (posOfAspect != -1) {
			var posOfNewAspect = (posOfAspect+1) % sigPossibleAspects.get(signalID).length;
			//console.log("Pos of new Aspect: " + posOfNewAspect);
			setSignalAjax(signalID, sigPossibleAspects.get(signalID)[posOfNewAspect]);
			sigAspectMap[String(signalID)] = sigPossibleAspects.get(signalID)[posOfNewAspect];
		}
	}, ()=>{});
}

function updatePossibleSignalAspects(signalID) {
	if (!sigPossibleAspects.has(signalID)) {
		return getPossibleSignalAspectsPromise(signalID, (resp) => {
			if (resp === undefined) {
				return;
			}
			var resp2 = new String(resp);
			// Remove all spaces, Split into array on ","
			var aspectsArr = resp2.replaceAll(" ", "").split(",");
			sigPossibleAspects.set(signalID, aspectsArr);
			console.log("Updated possible aspects for " + signalID);
		}, (resp) => {
			console.log("Could not update possible aspects for " + signalID);
		});
	}
	return new Promise((resolve, reject) => {
		resolve();
	})
}

function switchSignal(signalID) {
	console.log("switchSignal: " + signalID);
	updatePossibleSignalAspects(signalID).then(()=>{
		return updateAndSetSignalToNextAspectPromise(signalID);
	}).then(()=>{
		updateSignalVisuals();
	});
}


function pointclick(id) {
	console.log("Point id " + id);
	switchPoint(id);
}
function signalclick(id) {
	console.log("Signal id " + id);
	switchSignal(id);
}

function updatePointVisuals() {
	$('[id^="point"]').each(function () {
		var idstr = new String($(this).prop("id"));
		var colr = 'red';
		if (ptAspectMap[idstr] === "reverse") {
			colr = 'blue';
		}
		$(this).css({
			fill: colr,
			fillOpacity: 0.2
		});
	});
}

function updateSignalVisuals() {
	$('circle[id^="signal"]').each(function () {
		var idstr = new String($(this).prop("id"));
		var colr = 'green';
		if (sigAspectMap[idstr] === "aspect_caution") {
			colr = 'yellow';
		} else if (sigAspectMap[idstr] === "aspect_stop") {
			colr = 'red';
		} else if (sigAspectMap[idstr] === "aspect_shunt") {
			colr = 'grey';
		}
		$(this).css({
			fill: colr,
			fillOpacity: 0.8
		});
	});
}

async function updatePointAspectsMapAndVisuals() {
	getPointsAspectsAndUpdateMap();
	await new Promise(r => setTimeout(r, 750));
	updatePointVisuals();
}

function updateSignalAspectsMapAndVisuals() {
	updateSignalsAspectsPromise(()=>{updateSignalVisuals()},()=>{});
}


$(document).ready(
	async function () {
		await new Promise(r => setTimeout(r, 150));
		updatePointAspectsMapAndVisuals()
		updateSignalAspectsMapAndVisuals();
		
		// Run the get-update-visualize every ... milliseconds	
		let updateInterval = setInterval(function() {
			updatePointAspectsMapAndVisuals()
		}, 5000);
		
	}
);