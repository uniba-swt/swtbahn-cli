var trackOutput = 'master';
var sessionId = 0;

const pointAspects = ['normal', 'reverse'];

const ptIdMapperArray = [
	["p1", "point1"],
	["p2", "point2"],
	["p3", "point3"],
	["p4", "point4"],
	["p5", "point5"],
	["p6", "point6"],
	["p7", "point7"],
	["p8", "point8"],
	["p9", "point9"],
	["p10", "point10"],
	["p11", "point11"],
	["p12", "point12"],
	["p13", "point13"],
	["p14", "point14"],
	["p15", "point15"],
	["p16", "point16"],
	["p17", "point17"],
	["p18a", "point18a"],
	["p18b", "point18b"],
	["p19", "point19"],
	["p20", "point20"],
	["p21", "point21"],
	["p22", "point22"],
	["p23", "point23"],
	["p24", "point24"],
	["p25", "point25"],
	["p26", "point26"],
	["p27", "point27"],
	["p28", "point28"],
	["p29", "point29"]
];

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
	["point18", ""],
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

const ptIdMap = new Map(ptIdMapperArray);
var ptAspectMap = new Map(ptAspectMapperArray);

const sigIdMapperArray = [
	["s1", "signal1"],
	["s2", "signal2"],
	["s3", "signal3"],
	["s4", "signal4a"],
	["s5", "signal5"],
	["s6", "signal6"],
	["s7", "signal7a"],
	["s8", "signal8"],
	["s9", "signal9"],
	["s10", "signal10"],
	["s11", "signal11"],
	["s12", "signal12"],
	["s13", "signal13"],
	["s14", "signal14"],
	["s15", "signal15"],
	["s16", "signal16"],
	["s17", "signal17"],
	["s18", "signal18a"],
	["s19", "signal19"],
	["s20", "signal20"],
	["s21", "signal21"],
	["s22", "signal22a"],
	["s23", "signal23"],
	["s24", "signal24"],
	["s25", "signal25"],
	["s26", "signal26"],
	["s27", "signal27"],
	["s28", "signal28"],
	["s29", "signal29"],
	["s30", "signal30"],
	["s31", "signal31"],
	["s32", "signal32"],
	["s33", "signal33"],
	["s34", "signal34"],
	["s35", "signal35a"],
	["s36", "signal36"],
	["s37", "signal37"],
	["s38", "signal38"],
	["s39", "signal39"],
	["s40", "signal40"],
	["s41", "signal41"],
	["s42", "signal42"],
	["s43", "signal43"],
	["s44", "signal44"],
	["s45", "signal45"],
	["s46", "signal46a"],
	["s47", "signal47"],
	["s48", "signal48a"],
	["s49", "signal49"],
	["s50", "signal50a"],
	["s51", "signal51"],
	["s52", "signal52"],
	["s53", "signal53a"]
];


const sigAspectMapperArray = [
	["s1", "signal1"],
	["s2", "signal2"],
	["s3", "signal3"],
	["s4", "signal4a"],
	["s5", "signal5"],
	["s6", "signal6"],
	["s7", "signal7a"],
	["s8", "signal8"],
	["s9", "signal9"],
	["s10", "signal10"],
	["s11", "signal11"],
	["s12", "signal12"],
	["s13", "signal13"],
	["s14", "signal14"],
	["s15", "signal15"],
	["s16", "signal16"],
	["s17", "signal17"],
	["s18", "signal18a"],
	["s19", "signal19"],
	["s20", "signal20"],
	["s21", "signal21"],
	["s22", "signal22a"],
	["s23", "signal23"],
	["s24", "signal24"],
	["s25", "signal25"],
	["s26", "signal26"],
	["s27", "signal27"],
	["s28", "signal28"],
	["s29", "signal29"],
	["s30", "signal30"],
	["s31", "signal31"],
	["s32", "signal32"],
	["s33", "signal33"],
	["s34", "signal34"],
	["s35", "signal35a"],
	["s36", "signal36"],
	["s37", "signal37"],
	["s38", "signal38"],
	["s39", "signal39"],
	["s40", "signal40"],
	["s41", "signal41"],
	["s42", "signal42"],
	["s43", "signal43"],
	["s44", "signal44"],
	["s45", "signal45"],
	["s46", "signal46a"],
	["s47", "signal47"],
	["s48", "signal48a"],
	["s49", "signal49"],
	["s50", "signal50a"],
	["s51", "signal51"],
	["s52", "signal52"],
	["s53", "signal53a"]
];

const sigIdMap = new Map(sigIdMapperArray);
sigAspectMap = new Map(sigAspectMapperArray);


function setPointAjax(pointId, pointPosition) {
	$.ajax({
		type: 'POST',
		url: '/controller/set-point',
		crossDomain: true,
		data: { 'point': pointId, 'state': pointPosition },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log('Point ' + pointId + ' set to ' + pointPosition);
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log('System not running or invalid position!');
		}
	});
}

function getPointsAspectsAndUpdate() {
	$.ajax({
		type: 'POST',
		url: '/monitor/points',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log('Got point states');
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
			console.log('Could not get point states');
		}
	});
}


function setPointToAspect(pointId, pointAspect) {
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

async function switchPoint(pointID) {
	getPointsAspectsAndUpdate();
	await new Promise(r => setTimeout(r, 1000));
	let aspect = "reverse";
	if (ptAspectMap.get(pointID) === "reverse") {
		aspect = "normal";
	}
	setPointToAspect(pointID, aspect);
	ptAspectMap[pointID] = aspect;
	updateVisuals();
}

function pointclick(id) {
	console.log("Point id " + id);
	switchPoint(id);
}
function signalclick(id) {
	console.log("Signal id " + id);
}

async function updateVisuals() {
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

$(document).ready(
	async function () {
		getPointsAspectsAndUpdate();
		await new Promise(r => setTimeout(r, 1000));
		updateVisuals();
	}
);