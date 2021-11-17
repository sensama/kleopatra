function get_files
{
    echo application-vnd-kde-kleopatra.xml
}

function po_for_file
{
    case "$1" in
       application-vnd-kde-kleopatra.xml)
           echo kleopatra_xml_mimetypes.po
       ;;
    esac
}

function tags_for_file
{
    case "$1" in
       application-vnd-kde-kleopatra.xml)
           echo comment
       ;;
    esac
}

