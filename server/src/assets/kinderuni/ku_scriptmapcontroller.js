var trackOutput = 'master';
var sessionId = 0;

const pointAspects = ['normal', 'reverse'];
var ptAspectMap = new Map([
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
	["point26", ""]
	/*,
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
	["point29", ""]*/
]);

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

function setPointPromise(pointID, pointAspect) {
	return $.ajax({
		type: 'POST',
		url: '/controller/set-point',
		crossDomain: true,
		data: { 'point': pointID, 'state': pointAspect },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log("set point " + pointID + " to " + pointAspect);
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log("Error when setting point " + pointID + " to " + pointAspect + ": " + responseData + errorThrown);
		}
	});
}

function setSignalPromise(signalID, signalAspect) {
	return $.ajax({
		type: 'POST',
		url: '/controller/set-signal',
		crossDomain: true,
		data: { 'signal': signalID, 'state': signalAspect },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log('Signal ' + signalID + ' set to ' + signalAspect);
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log("Error when setting signal " + signalID + " to " + signalAspect + ": " + responseData + errorThrown);
		}
	});
}

function getMonitorPostRequestPromiseSEP(url, onSuccess, onErr) {
	return $.ajax({
		type: 'POST',
		url: url,
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

function updateParamAspectsPromise(isPoint) {
	const url = isPoint ? '/monitor/points' : '/monitor/signals';
	return getMonitorPostRequestPromiseSEP(
		url, 
		// Success Callback with Param
		(res) => {
			const aspRegex = /(\w+) - state: (\w+)/g;
			var respSplit = res.split(/\r?\n/);
			for (const elem of respSplit) {
				var matchArr = [...elem.matchAll(aspRegex)];
				if (matchArr !== null) {
					if (isPoint) {
						ptAspectMap[matchArr[0][1]] = matchArr[0][2];
					} else {
						sigAspectMap[matchArr[0][1]] = matchArr[0][2];
					}
				}
			}
		},
		// Err Callback with Param
		(res) => {
			console.log('Could not get aspects via ' + url + ". " + res);
		}
	);
}



function getPossibleSignalAspectsPromiseSEP(signalID, onSuccess, onErr) {
	return $.ajax({
		type: 'POST',
		url: '/monitor/signal-aspects',
		crossDomain: true,
		data: {
			'signal': signalID,
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

function updatePossibleSignalAspectsPromise(signalID) {
	if (!sigPossibleAspects.has(signalID)) {
		return getPossibleSignalAspectsPromiseSEP(
			signalID,
			// Success
			(response) => {
				if (response === undefined || response === null) {
					return;
				}
				/** @type String */
				var responseStr = new String(response);
				// Remove all spaces, Split into array on ","
				responseStr = responseStr.replaceAll(" ", "");
				
				// KinderUni special: remove aspect_caution if it exists.
				if (responseStr.includes("aspect_caution,")) {
					responseStr = responseStr.replace("aspect_caution,", "");
				}
				// KinderUni special: remove aspect_shunt if aspect_go also exists.
				if (responseStr.includes("aspect_shunt,") && responseStr.includes("aspect_go")) {
					responseStr = responseStr.replace("aspect_shunt,", "");
				}
				console.log("Possible aspects for " + signalID + ": " + responseStr);
				/** @type String[] */
				var aspectsArr = responseStr.split(",");
				sigPossibleAspects.set(signalID, aspectsArr);
			},
			// Err
			(response) => {
				console.log("Could not update possible aspects for " + signalID + ". " + response);
			}
		);
	}
	// Possible aspects of signalID already in Map -> resolved.
	return new Promise((resolve, reject) => {
		resolve();
	})
}

function getNextAspectOfSignal(signalID) {
	if (signalID === undefined || !sigAspectMap.has(signalID)) {
		return "aspect_stop";
	}
	var currAspect = sigAspectMap[String(signalID)];
	var posOfAspect = sigPossibleAspects.get(signalID).findIndex((elem) => elem === currAspect);
	if (posOfAspect != -1) {
		var posOfNewAspect = (posOfAspect+1) % sigPossibleAspects.get(signalID).length;
		return sigPossibleAspects.get(signalID)[posOfNewAspect];
	} else {
		return sigPossibleAspects.get(signalID)[0];
	}
}

function updatePointVisualsSelector(selector) {
	$(selector).each(function () {
		var idstr = new String($(this).prop("id"));
		//console.log("updatePointVisualsSelector Element ID: " + idstr)
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

function updateSignalVisualsSelector(selector) {
	$(selector).each(function () {
		var idstr = new String($(this).prop("id"));
		var colr = 'green';
		if (sigAspectMap[idstr] === "aspect_caution") {
			colr = 'yellow';
		} else if (sigAspectMap[idstr] === "aspect_stop") {
			colr = 'red';
		} else if (sigAspectMap[idstr] === "aspect_shunt") {
			///TODO: Decide if for Kinderuni, we want this to also show green instead.
			colr = 'grey';
		}
		$(this).css({
			fill: colr,
			fillOpacity: 0.8
		});
	});
}

function updatePointsVisuals() {
	//updatePointVisualsSelector('[id^="point"]');
	// Need to also set the default arrows
	for (const [key, value] of ptAspectMap) {
		updatePointVisuals(key);
	}
}

function updatePointVisuals(pointID) {
	//updatePointVisualsSelector('[id="' + pointID + '"]');
	var shortPointId = "p" + pointID.substring(5);
	indicate_point_state(shortPointId, ptAspectMap[pointID]);
}

function updateSignalsVisuals() {
	updateSignalVisualsSelector('circle[id^="signal"]');
}

function updateSignalVisuals(signalID) {
	updateSignalVisualsSelector('circle[id="' + signalID + '"]');
}


function switchPoint(pointID) {
	var next_aspect = "reverse";
	updateParamAspectsPromise(true)
			.then(() => {
				if (ptAspectMap[pointID] === "reverse") {
					next_aspect = "normal";
				}
			}).then(() => setPointPromise(pointID, next_aspect))
			.then(() => {
				ptAspectMap[pointID] = next_aspect;
			})
			.then(() => updatePointVisuals(pointID));
}

function switchSignal(signalID) {
	var next_aspect = "aspect_stop";
	// 1. update current aspect of signal (updateParamAspectsPromise)
	// 2. ensure that we know what aspects the signal supports (updatePossibleSignalAspectsPromise)
	// 3. determine the next aspect that we will now switch to (getNextAspectOfSignal)
	// 4. set the signal aspect via a call to the server (setSignalPromise)
	// 5. update the internal current aspect map (sigAspectMap[signalID] = next_aspect)
	// 6. update the visual display of the signal
	updateParamAspectsPromise(false)
			.then(() => updatePossibleSignalAspectsPromise(signalID))
			.then(() => {
				next_aspect = getNextAspectOfSignal(signalID);
				return setSignalPromise(signalID, next_aspect);
			}).then(() => {
				sigAspectMap[signalID] = next_aspect;
			})
			.then(() => updateSignalVisuals(signalID));
}

function pointclick(id) {
	console.log("Point id " + id);
	switchPoint(id);
}
function signalclick(id) {
	console.log("Signal id " + id);
	switchSignal(id);
}

function adjustRailStroke(strokeFloat) {
	const selector_goal = '[id*="rail"]';
	$(selector_goal).each(function () {
		$(this).css({
			stroke: "#000",
			strokeWidth: 0.5
		});
	});
}


function fadePointPortion(selector) {
	$(selector).each(function () {
		var idstr = new String($(this).prop("id"));
		if (idstr.includes("rail") || idstr.includes("arrow")) {
			$(this).css({
				stroke: "#777",
				strokeWidth: 0.1
			});
		} else {
			$(this).css({
				fillOpacity: 0.1
			});
		}
	});
}

function unfadePointPortion(selector) {
	$(selector).each(function () {
		var idstr = new String($(this).prop("id"));
		if (idstr.includes("rail")) {
			$(this).css({
				stroke: "#000",
				strokeWidth: 0.5
			});
		} else if (idstr.includes("arrow")) {
			$(this).css({
				stroke: "#000",
				strokeWidth: 1.2
			});
		} else {
			$(this).css({
				fillOpacity: 1.0
			});
		}
	});
}


function indicate_point_state(pointShortId, position) {
	console.log("Indicate " + pointShortId + " to be in " + position);
	const p_state_goal = position == "normal" ? "straight" : "branch";
	const p_state_other = position == "normal" ? "branch" : "straight";
	const selector_goal = '[id^="' + pointShortId + '_' + p_state_goal + '"]';
	const selector_other = '[id^="' + pointShortId + '_' + p_state_other + '"]';
	//console.log("Selector Goal: " + selector_goal + "; other: " + selector_other);
	unfadePointPortion(selector_goal);
	fadePointPortion(selector_other);
}


$(document).ready(
	async function () {
		await new Promise(r => setTimeout(r, 350));
		updateParamAspectsPromise(true)
		.then(() => updatePointsVisuals());
		updateParamAspectsPromise(false)
		.then(() => updateSignalsVisuals());
		
		// Adjust the appearance of all rails.
		adjustRailStroke();
		// Periodically query the point & signal aspects
		let updateInterval = setInterval(function() {
			updateParamAspectsPromise(true)
					.then(() => updatePointsVisuals());
			updateParamAspectsPromise(false)
					.then(() => updateSignalsVisuals());
		}, 2000);
	}
);