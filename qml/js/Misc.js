function elapsedTime(date) {
    var txt = Format.formatDate(date, Formatter.Timepoint)
    var elapsed = Format.formatDate(date, Formatter.DurationElapsed)
    return txt + (elapsed ? ' (' + elapsed + ')' : '')
}

function messageDate(date) {
    return (new Date() - date < 300000) ? '' : Format.formatDate(date, Formatter.TimePoint)
}

function openUrl(link) {
    Qt.openUrlExternally(link)
}
