var url = '';
var trackOutput = 'master';
var sessionId = 0;
var grabId = -1;
var trainId = '';
var trainEngine = '';
var routeId = -1;
$(document).ready(function() {

        trackOutput = 'master';

        $('#pingButton').click(function() {
            $('#pingResponse').text('Waiting');
            $.ajax({
                type: 'POST',
                url: url,
                crossDomain: true,
                data: null,
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#pingResponse').parent().removeClass('alert-danger');
                    $('#pingResponse').parent().addClass('alert-success');
                    $('#pingResponse').text('OK');
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#pingResponse').parent().removeClass('alert-success');
                    $('#pingResponse').parent().addClass('alert-danger');
                    $('#pingResponse').text('Error');
                }
            });
        });

        $('#startupButton').click(function() {
            $('#startupShutdownResponse').text('Waiting');
            $.ajax({
                type: 'POST',
                url: url + '/admin/startup',
                crossDomain: true,
                data: null,
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#startupShutdownResponse').parent().removeClass('alert-danger');
                    $('#startupShutdownResponse').parent().addClass('alert-success');
                    $('#startupShutdownResponse').text('OK');
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#startupShutdownResponse').parent().removeClass('alert-success');
                    $('#startupShutdownResponse').parent().addClass('alert-danger');
                    $('#startupShutdownResponse').text('System already running!');
                }
            });
        });

        $('#shutdownButton').click(function() {
            $('#startupShutdownResponse').text('Waiting');
            $.ajax({
                type: 'POST',
                url: url + '/admin/shutdown',
                crossDomain: true,
                data: null,
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#startupShutdownResponse').parent().removeClass('alert-danger');
                    $('#startupShutdownResponse').parent().addClass('alert-success');
                    $('#startupShutdownResponse').text('OK');
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#startupShutdownResponse').parent().removeClass('alert-success');
                    $('#startupShutdownResponse').parent().addClass('alert-danger');
                    $('#startupShutdownResponse').text('System not running!');
                }
            });
        });

        $('#grabTrainButton').click(function() {
            $('#grabTrainResponse').text('Waiting');
            trainId = $('#grabTrainId option:selected').text();
            trainEngine = $('#grabEngine option:selected').text();
            if (sessionId == 0 && grabId == -1) {
                $.ajax({
                    type: 'POST',
                    url: url + '/driver/grab-train',
                    crossDomain: true,
                    data: { 'train': trainId, 'engine': trainEngine },
                    dataType: 'text',
                    success: function(responseData, textStatus, jqXHR) {
                        responseDataSplit = responseData.split(',');
                        sessionId = responseDataSplit[0];
                        grabId = responseDataSplit[1];
                        $('#sessionGrabId')
                            .text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
                        $('#grabTrainResponse').text('Grabbed');
                        $('#grabTrainResponse').parent().removeClass('alert-danger');
                        $('#grabTrainResponse').parent().addClass('alert-success');
                    },
                    error: function(responseData, textStatus, errorThrown) {
                        $('#grabTrainResponse')
                            .text('System not running or train not available!');
                        $('#grabTrainResponse').parent().addClass('alert-danger');
                        $('#grabTrainResponse').parent().removeClass('alert-success');
                    }
                });
            } else {
                $('#grabTrainResponse').text('You can only grab one train!');
            }
        });

        $('#releaseTrainButton').click(function() {
            $('#grabTrainResponse').text('Waiting');
            if (sessionId != 0 && grabId != -1) {
                $.ajax({
                    type: 'POST',
                    url: url + '/driver/release-train',
                    crossDomain: true,
                    data: { 'session-id': sessionId, 'grab-id': grabId },
                    dataType: 'text',
                    success: function(responseData, textStatus, jqXHR) {
                        sessionId = 0;
                        grabId = -1;
                        $('#sessionGrabId')
                            .text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
                        $('#grabTrainResponse').text('Released grabbed train');
                        $('#grabTrainResponse').parent().removeClass('alert-danger');
                        $('#grabTrainResponse').parent().addClass('alert-success');
                    },
                    error: function(responseData, textStatus, errorThrown) {
                        $('#grabTrainResponse').parent().addClass('alert-danger');
                        $('#grabTrainResponse').parent().removeClass('alert-success');
                        $('#grabTrainResponse').text('System not running!');
                    }
                });
            } else {
                $('#grabTrainResponse').parent().addClass('alert-danger');
                $('#grabTrainResponse').parent().removeClass('alert-success');
                $('#grabTrainResponse').text('No grabbed train!');
            }
        });

        speeds = [
            "0",
            "10",
            "20",
            "30",
            "40",
            "50",
            "60",
            "70",
            "80",
            "90",
            "100",
            "110",
            "120",
            "126"
        ]

        $('#speedMinus').click(function() {
            speed = $('#dccSpeed').val();
            position = speeds.indexOf(speed);
            if (position > 0) {
                $('#dccSpeed:text').val(speeds[position - 1]);
            }
        });

        $('#speedPlus').click(function() {
            speed = $('#dccSpeed').val();
            position = speeds.indexOf(speed);
            if (position < speeds.length - 1) {
                $('#dccSpeed:text').val(speeds[position + 1]);
            }
        });

        $('#driveTrainButton').click(function() {
            $('#driveTrainResponse').text('Waiting');
            speed = $('#dccSpeed').val();
            direction = $('#direction').is(':checked');
            speed = direction ? speed : -speed;
            if (sessionId != 0 && grabId != -1) {
                $.ajax({
                    type: 'POST',
                    url: url + '/driver/set-dcc-train-speed',
                    crossDomain: true,
                    data: {
                        'session-id': sessionId,
                        'grab-id': grabId,
                        'speed': speed,
                        'track-output': trackOutput
                    },
                    dataType: 'text',
                    success: function(responseData, textStatus, jqXHR) {
                        $('#driveTrainResponse').text('DCC train speed set to ' + speed);
                        $('#driveTrainResponse').parent().removeClass('alert-danger');
                        $('#driveTrainResponse').parent().addClass('alert-success');
                    },
                    error: function(responseData, textStatus, errorThrown) {
                        $('#driveTrainResponse')
                            .text('System not running or invalid track output!');
                        $('#driveTrainResponse').parent().addClass('alert-danger');
                        $('#driveTrainResponse').parent().removeClass('alert-success');
                    }
                });
            } else {
                $('#driveTrainResponse').text('You must have a grabbed train!');
                $('#driveTrainResponse').parent().addClass('alert-danger');
                $('#driveTrainResponse').parent().removeClass('alert-success');
            }
        });

        $('#stopTrainButton').click(function() {
            speed = $('#dccSpeed').val();
            $('#dccSpeed').val(0);
            $('#driveTrainButton').click();
            $('#dccSpeed').val(speed);
        });

        // $('#requestRouteButton').click(function() {
        //     $('#routeResponse').text('Waiting');
        //     source = $('#source').val();
        //     destination = $('#destination').val();
        //     if (sessionId != 0 && grabId != -1) {
        //         $.ajax({
        //             type: 'POST',
        //             url: url + '/driver/request-route',
        //             crossDomain: true,
        //             data: {
        //                 'session-id': sessionId,
        //                 'grab-id': grabId,
        //                 'source': source,
        //                 'destination': destination
        //             },
        //             dataType: 'text',
        //             success: function(responseData, textStatus, jqXHR) {
        //                 routeId = responseData;
        //                 $('#routeResponse').text('Route ' + responseData + ' granted');
        //             },
        //             error: function(responseData, textStatus, errorThrown) {
        //                 $('#routeResponse').text('Route not granted!');
        //             }
        //         });
        //     } else {
        //         $('#routeResponse').text('You must have a grabbed train!');
        //     }
        // });

        // $('#driveRouteButton').click(function() {
        //     $('#routeResponse').text('Waiting');
        //     if (sessionId != 0 && grabId != -1) {
        //         $.ajax({
        //             type: 'POST',
        //             url: url + '/driver/drive-route',
        //             crossDomain: true,
        //             data: { 'session-id': sessionId, 'grab-id': grabId, 'route-id': routeId },
        //             dataType: 'text',
        //             success: function(responseData, textStatus, jqXHR) {
        //                 $('#routeResponse')
        //                     .text('Route ' + responseData + ' driving completed');
        //             },
        //             error: function(responseData, textStatus, errorThrown) {
        //                 $('#routeResponse').text('Route could not be driven!');
        //             }
        //         });
        //     } else {
        //         $('#routeResponse').text('You must have a grabbed train!');
        //     }
        // });

        // $('#releaseRouteButton').click(function() {
        //     $('#releaseRouteResponse').text('Waiting');
        //     var routeId = $('#routeId').val();
        //     $.ajax({
        //         type: 'POST',
        //         url: url + '/controller/release-route',
        //         crossDomain: true,
        //         data: { 'route-id': routeId },
        //         dataType: 'text',
        //         success: function(responseData, textStatus, jqXHR) {
        //             $('#releaseRouteResponse').text('Route ' + routeId + ' released');
        //         },
        //         error: function(responseData, textStatus, errorThrown) {
        //             $('#releaseRouteResponse')
        //                 .text('System not running or invalid track output!');
        //         }
        //     });
        // });

        points = [
            'point1',
            'point2',
            'point3',
            'point4',
            'point5',
        ]

        $('#pointMinus').click(function() {
            point = $('#pointId').val();
            position = points.indexOf(point);
            if (position > 0) {
                $('#pointId:text').val(points[position - 1]);
            }
        });

        $('#pointPlus').click(function() {
            point = $('#pointId').val();
            position = points.indexOf(point);
            if (position < points.length - 1) {
                $('#pointId:text').val(points[position + 1]);
            }
        });

        $('.pointBtn').click(function() {
            $('#setPointResponse').text('Waiting');
            var pointId = $('#pointId').val();
            var pointPosition = $(this).text();

            $.ajax({
                type: 'POST',
                url: url + '/controller/set-point',
                crossDomain: true,
                data: { 'point': pointId, 'state': pointPosition },
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#setPointResponse')
                        .text('Point ' + pointId + ' set to ' + pointPosition);
                    $('#setPointResponse').parent().removeClass('alert-danger');
                    $('#setPointResponse').parent().addClass('alert-success');
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#setPointResponse').text('System not running or invalid position!');
                    $('#setPointResponse').parent().removeClass('alert-success');
                    $('#setPointResponse').parent().addClass('alert-danger');
                }
            });
        });

        signals = [
            'signal1',
            'signal2',
            'signal3',
            'signal4',
            'signal5',
        ]

        $('#signalMinus').click(function() {
            signal = $('#signalId').val();
            position = signals.indexOf(signal);
            if (position > 0) {
                $('#signalId:text').val(signals[position - 1]);
            }
        });

        $('#signalPlus').click(function() {
            signal = $('#signalId').val();
            position = signals.indexOf(signal);
            if (position < signals.length - 1) {
                $('#signalId:text').val(signals[position + 1]);
            }
        });

        $('.signalBtn').click(function() {
            $('#setSignalResponse').text('Waiting');
            var signalId = $('#signalId').val();
            var signalAspect = $(this).text();
            $.ajax({
                type: 'POST',
                url: url + '/controller/set-signal',
                crossDomain: true,
                data: { 'signal': signalId, 'state': signalAspect },
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#setSignalResponse')
                        .text('Signal ' + signalId + ' set to ' + signalAspect);
                    $('#setSignalResponse').parent().removeClass('alert-danger');
                    $('#setSignalResponse').parent().addClass('alert-success');
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#setSignalResponse').text('System not running or invalid aspect!');
                    $('#setSignalResponse').parent().removeClass('alert-success');
                    $('#setSignalResponse').parent().addClass('alert-danger');
                }
            });
        });
    }

);